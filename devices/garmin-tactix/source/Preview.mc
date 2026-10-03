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
import Toybox.Position;
import Toybox.System;
import Toybox.Time;
import Toybox.WatchUi;
import Toybox.Weather;

// Preview build only (preview.jungle): fixed scenarios for the simulator, so
// the home screen, the Recon page and the Controls can be looked at in each
// state without a LayerWand, weather, or GPS. A tap on the LAYERTIME title
// moves to the next scenario and names it in a toast. Nothing here is in
// the release or test builds.
//
//   1 NORMAL        LayerWand connected, early warning, 12 detections,
//                   fresh weather, GPS fix
//   2 ALERT         manual ALL, an unacknowledged alert, 3 detections
//   3 DISCONNECTED  the LayerWand link lost, reconnecting; last counts kept
//   4 STALE WEATHER connected; the weather observation is 5 hours old
//   5 MISSING DATA  no LayerWand yet, no weather, no GPS
(:preview)
module Preview {

    const NAMES = ["NORMAL", "ALERT", "DISCONNECTED", "STALE WEATHER", "MISSING DATA"] as Array<String>;

    var scenario as Number = 0;
    var link as LinkClient? = null;

    function begin(l as LinkClient) as Void {
        link = l;
        apply();
    }

    function next() as Void {
        scenario = (scenario + 1) % NAMES.size();
        apply();
        if (WatchUi has :showToast) {
            WatchUi.showToast("PREVIEW " + (scenario + 1) + " " + NAMES[scenario], null);
        }
        WatchUi.requestUpdate();
    }

    function apply() as Void {
        var l = link;
        if (l == null) { return; }
        var now = System.getTimer();
        l.mirror.reset(0x3FA2);
        l.sessionId = 0x3FA2;
        l.newSession = false;
        l.lastStatusMs = now;
        l.heartbeat = 42;
        l.nodeName = "LT-C5-F412";
        l.lastAlertEventId = 0;
        if (scenario == 1) {
            l.flags = Link.FLAG_MONITORING | Link.FLAG_EARLY_WARNING_ENABLED | Link.FLAG_ALERT_PENDING;
            l.selected = 1;
            l.active = 5;
            events(l, 3, now);
            l.lastAlertEventId = 3;
        } else if (scenario == 4) {
            l.flags = 0;
            l.selected = 0;
            l.active = 0;
            l.eventCount = 0;
            l.lastStatusMs = 0;
            l.sessionId = 0;
        } else {
            l.flags = Link.FLAG_EARLY_WARNING_ENABLED;
            l.selected = 0;
            l.active = 17;
            events(l, 12, now);
        }
    }

    // n detections, newest last, as GET_CHANGED would have delivered them.
    function events(l as LinkClient, n as Number, now as Number) as Void {
        var detectors = [5, 10, 11, 7, 6, 9, 12, 14, 2, 15, 16, 13] as Array<Number>;
        var rssi = [-67, -81, -54, -90, -72, -63, -88, -77, -59, -95, -70, -84] as Array<Number>;
        var counts = [12, 1, 3, 40, 2, 7, 1, 5, 1, 2, 9, 1] as Array<Number>;
        for (var i = 0; i < n; i++) {
            var k = i % detectors.size();
            l.mirror.apply({
                :eventId => (i + 1).toLong(), :detector => detectors[k], :confidence => 2 - (i % 3),
                :sourceKind => detectors[k] >= 10 ? 2 : 1, :band => detectors[k] >= 10 ? 0 : 1,
                :channel => detectors[k] >= 10 ? 0 : 1 + (i * 5) % 11, :rssi => rssi[k], :count => counts[k],
                :ageSeconds => (n - i) * 37, :flags => 3
            }, now);
        }
        l.mirror.complete({:changeSeq => 20l, :gap => 0});
        l.mirror.setText(n, Link.TEXT_FIELD_SOURCE_ID, "A4:CF:12:9B:3E:71");
        l.mirror.setText(n, Link.TEXT_FIELD_DETAIL, "Deauth burst from Free Public WiFi");
        l.eventCount = n;
        l.changeSeq = 20;
    }

    function phase() as Symbol {
        if (scenario == 2) { return :reconnecting; }
        if (scenario == 4) { return :searching; }
        return :ready;
    }

    function weather() as Dictionary? {
        if (scenario == 4) { return null; }
        var observed = Time.now();
        if (scenario == 3) { observed = observed.subtract(new Time.Duration(5 * 3600)); }
        return {
            :temperature => 18.4,
            :condition => scenario == 3 ? Weather.CONDITION_RAIN : Weather.CONDITION_PARTLY_CLOUDY,
            :observed => observed,
            :location => here()
        };
    }

    function here() as Position.Location {
        return new Position.Location({:latitude => 50.8503, :longitude => 4.3517, :format => :degrees});
    }
}

module Env {

    (:preview)
    function startLink(link as LinkClient) as Void { Preview.begin(link); }

    (:preview)
    function stopLink(link as LinkClient) as Void {}

    (:preview)
    function weather() as Dictionary? { return Preview.weather(); }

    (:preview)
    function hasFix(nav as NavTracker) as Boolean { return Preview.scenario != 4; }

    (:preview)
    function location(nav as NavTracker) as Position.Location? {
        return Preview.scenario != 4 ? Preview.here() : null;
    }

    (:preview)
    function forcedPhase() as Symbol? { return Preview.phase(); }

    (:preview)
    function titleTapped(link as LinkClient) as Boolean {
        Preview.next();
        return true;
    }
}
