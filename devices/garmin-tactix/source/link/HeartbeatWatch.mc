// LayerTime - passive early-warning system. Connect IQ Device App for the
// Garmin tactix 8 AMOLED.
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import Toybox.Lang;
import Toybox.System;
import Toybox.Timer;
import Toybox.WatchUi;

// Test-only 10-minute Status stability measurement (Increment 1 acceptance
// item (b)): "heartbeat advances, no gap over 2 s over 10 minutes". Built
// only by linktest.jungle; the release build keeps the (:production) stub.
//
// DOWN starts it while Connected; a second DOWN while it runs is ignored so
// an accidental press cannot spoil a 10-minute evidence run. It ends itself
// at the 600 s boundary. Rules of the measurement: see HeartbeatRun.mc.
//
// This module sends nothing over BLE. It records Status receipt times from
// LinkClient.statusObserver (inside the BLE callback, record only, finding
// F1), watches LinkClient.disconnects for interruptions, and checks the
// window from its own one-second timer. Log lines go to System.println
// (GARMIN/Apps/Logs/LayerTime.TXT when that file exists): hb,start;
// hb,gap,<n>,<ms> for every interval over 2000 ms; hb,disconnect,<count>;
// hb,end,PASS|FAIL,... Normal one-second intervals are not logged.
(:linktest)
module HeartbeatWatch {
    const TICK_MS = 1000;

    var _link as LinkClient? = null;
    var _run as HeartbeatRun? = null;
    var _timer as Timer.Timer? = null;
    var _running as Boolean = false;
    var _disconnectsAtStart as Number = 0;
    var _disconnectsSeen as Number = 0;
    var _summary as Dictionary? = null;
    var _startedMs as Number = 0;

    function isAvailable() as Boolean { return true; }
    function isRunning() as Boolean { return _running; }

    // DOWN: starts the measurement if the link is ready; ignored otherwise
    // and ignored while a run is in progress. Returns true when started.
    function start(link as LinkClient) as Boolean {
        if (_running || !link.isReady()) { return false; }
        _link = link;
        _run = new HeartbeatRun();
        if (_timer == null) { _timer = new Timer.Timer(); }
        _disconnectsAtStart = link.disconnects;
        _disconnectsSeen = link.disconnects;
        _summary = null;
        _startedMs = System.getTimer();
        _running = true;
        link.statusObserver = new Lang.Method(HeartbeatWatch, :onStatus);
        (_timer as Timer.Timer).start(new Lang.Method(HeartbeatWatch, :onTick), TICK_MS, true);
        System.println("hb,start");
        WatchUi.requestUpdate();
        return true;
    }

    // From LinkClient, inside the BLE notification callback: record only.
    function onStatus(receivedMs as Number) as Void {
        var run = _run;
        if (!_running || run == null) { return; }
        var gap = run.onStatus(receivedMs);
        if (gap > run.gapLimitMs()) {
            System.println("hb,gap," + run.intervals() + "," + gap);
        }
    }

    // Own timer, on the event loop: disconnect bookkeeping and the boundary.
    function onTick() as Void {
        var run = _run;
        var link = _link;
        if (!_running || run == null || link == null) { return; }
        while (_disconnectsSeen < link.disconnects) {
            _disconnectsSeen++;
            run.noteDisconnect();
            System.println("hb,disconnect," + run.disconnects());
        }
        if (run.tick(System.getTimer())) { finish(); return; }
        WatchUi.requestUpdate();
    }

    function finish() as Void {
        _running = false;
        if (_timer != null) { (_timer as Timer.Timer).stop(); }
        if (_link != null) { (_link as LinkClient).statusObserver = null; }
        var run = _run;
        if (run == null) { return; }
        var s = run.summary();
        _summary = s;
        System.println("hb,end," + ((s[:pass] as Boolean) ? "PASS" : "FAIL") +
            ",duration=" + (s[:duration] as Number) + ",received=" + (s[:received] as Number) +
            ",intervals=" + (s[:intervals] as Number) + ",min=" + (s[:min] as Number) +
            ",mean=" + (s[:mean] as Number) + ",p95=" + (s[:p95] as Number) + ",max=" + (s[:max] as Number) +
            ",gaps=" + (s[:gaps] as Number) + ",disconnects=" + (s[:disconnects] as Number));
        WatchUi.requestUpdate();
    }

    // One compact line while running, three lines when complete.
    function statusLines() as Array<String> {
        var run = _run;
        if (_running && run != null) {
            if (!run.hasEpoch()) { return ["HB waiting for Status"] as Array<String>; }
            var el = run.elapsedMs(System.getTimer()) / 1000;
            return ["HB " + el + "/600 s  n=" + run.received() + "  max " + run.maxInterval() +
                    (run.disconnects() > 0 ? "  dc " + run.disconnects() : "")] as Array<String>;
        }
        var s = _summary;
        if (s == null) { return [] as Array<String>; }
        return [
            "HB " + ((s[:pass] as Boolean) ? "PASS" : "FAIL") + "  " + ((s[:duration] as Number) / 1000) + " s  n=" + (s[:received] as Number),
            "gap " + (s[:min] as Number) + "/" + (s[:mean] as Number) + "/" + (s[:p95] as Number) + "/" + (s[:max] as Number) + " ms",
            ">2 s: " + (s[:gaps] as Number) + "  disconnects " + (s[:disconnects] as Number)
        ] as Array<String>;
    }

    function hint() as String { return "START pings. UP bursts 100. DOWN watches 10 min. BACK exits."; }
}

// The release build's stand-in: no measurement, no hook, nothing on screen.
(:production)
module HeartbeatWatch {
    function isAvailable() as Boolean { return false; }
    function isRunning() as Boolean { return false; }
    function start(link as LinkClient) as Boolean { return false; }
    function statusLines() as Array<String> { return [] as Array<String>; }
    function hint() as String { return "START pings. BACK exits."; }
}
