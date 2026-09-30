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

// Test-only 100-PING burst for the Increment 1 acceptance measurement (c).
// Built only by linktest.jungle; monkey.jungle (the release build) excludes
// the (:linktest) module and keeps the (:production) stub, so UP does
// nothing extra in the product.
//
// Pacing is back to back with no measurement interval: the next PING is
// issued only after the previous one has completed (matched ACK, or the
// existing 3 s timeout, token mismatch or ERROR), which LinkClient reports
// through pingObserver. It is NOT issued from inside that report: the
// report arrives in the BLE notification callback, and a GATT write started
// there crashed on the watch (Part 006-B4542-00, firmware 27.18, at
// LinkClient.sendNext). onPing only records the result; a one-shot timer at
// the host minimum (50 ms) hands control back to the event loop, and the
// timer callback issues the PING, once the stack has finished the previous
// request (LinkClient.requestPending, findings F1 and F2). One request is
// outstanding at any time by
// construction. Every observation is printed with System.println, which
// Connect IQ writes to GARMIN/Apps/Logs/LayerTime.TXT when that file
// exists.
(:linktest)
module LinkBurst {
    const TARGET = 100;
    const YIELD_MS = 50; // Timer minimum on the host system, per the API reference

    var _link as LinkClient? = null;
    var _run as LinkBurstRun? = null;
    var _timer as Timer.Timer? = null;
    var _running as Boolean = false;
    var _summary as Dictionary? = null;
    var _why as String = "";

    function isAvailable() as Boolean { return true; }
    function isRunning() as Boolean { return _running; }

    // UP: starts the burst, or stops one that is running.
    function toggle(link as LinkClient) as Boolean {
        if (_running) { finish("stopped"); return true; }
        _link = link;
        _run = new LinkBurstRun(TARGET);
        if (_timer == null) { _timer = new Timer.Timer(); }
        link.pingObserver = new Lang.Method(LinkBurst, :onPing);
        _running = true;
        _summary = null;
        System.println("burst,start," + TARGET);
        scheduleSend();
        return true;
    }

    // From LinkClient, inside a BLE callback: record only, never send here.
    function onPing(rttMs as Number) as Void {
        var run = _run;
        if (!_running || run == null) { return; }
        var more = run.record(rttMs);
        System.println("ping," + run.sent() + "," + (rttMs >= 0 ? rttMs.toString() : "lost"));
        if (!more) { finish("done"); return; }
        scheduleSend();
        WatchUi.requestUpdate();
    }

    function scheduleSend() as Void {
        var t = _timer;
        if (t != null) { t.start(new Lang.Method(LinkBurst, :sendOne), YIELD_MS, false); }
    }

    // Timer callback, on the event loop.
    function sendOne() as Void {
        var link = _link;
        var run = _run;
        if (!_running || link == null || run == null) { return; }
        if (link.requestPending) {
            // The previous write's completion has not been delivered yet;
            // give the stack another turn rather than start a second write.
            scheduleSend();
            return;
        }
        if (!link.ping()) { finish("link not ready"); return; }
        run.noteSent();
    }

    function finish(why as String) as Void {
        _running = false;
        _why = why;
        if (_timer != null) { (_timer as Timer.Timer).stop(); }
        if (_link != null) { (_link as LinkClient).pingObserver = null; }
        var run = _run;
        var s = run != null ? run.summary() : LinkBurstStats.compute([] as Array<Number>);
        _summary = s;
        System.println("burst," + why + ",sent=" + (s[:sent] as Number) + ",acked=" + (s[:acked] as Number) +
            ",lost=" + (s[:lost] as Number) + ",min=" + (s[:min] as Number) + ",median=" + (s[:median] as Number) +
            ",mean=" + (s[:mean] as Number) + ",p95=" + (s[:p95] as Number) + ",max=" + (s[:max] as Number));
        WatchUi.requestUpdate();
    }

    // What the view shows: one line while running, three lines afterwards.
    function statusLines() as Array<String> {
        var run = _run;
        if (_running && run != null) {
            return ["Burst " + run.sent() + "/" + TARGET + "  lost " + run.lost()] as Array<String>;
        }
        var s = _summary;
        if (s == null) { return [] as Array<String>; }
        return [
            (s[:sent] as Number).toString() + " sent  " + (s[:acked] as Number).toString() + " ack  " + (s[:lost] as Number).toString() + " lost",
            "RTT " + (s[:min] as Number) + "/" + (s[:median] as Number) + "/" + (s[:max] as Number) + " ms",
            "mean " + (s[:mean] as Number) + "  p95 " + (s[:p95] as Number) + "  (" + _why + ")"
        ] as Array<String>;
    }

    function hint() as String { return "START pings. UP bursts 100. BACK exits."; }
}

// The release build's stand-in: no burst, no hook, nothing on screen.
(:production)
module LinkBurst {
    function isAvailable() as Boolean { return false; }
    function isRunning() as Boolean { return false; }
    function toggle(link as LinkClient) as Boolean { return false; }
    function statusLines() as Array<String> { return [] as Array<String>; }
    function hint() as String { return "START pings. BACK exits."; }
}
