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

import Toybox.Graphics;
import Toybox.Lang;
import Toybox.System;
import Toybox.WatchUi;

// The Increment 1 transport view: link state, Node name, heartbeat, session,
// PING round trip, and the probe measurement. This is the transport proof,
// not the product UI (decision D4 stays open). Reached from the Controls
// (Link diagnostics). The PING button at the bottom is the one touch target
// on this page: a tap on it sends a PING, as START does.
class LayerTimeView extends WatchUi.View {

    private const CYAN = 0x30D0FF;

    private var _link as LinkClient;
    // The PING button of the last draw, [x, y, w, h].
    private var _pingBox as Array<Number> = [0, 0, 0, 0];

    // True when (x, y) is on the PING button.
    public function inPing(x as Number, y as Number) as Boolean { return HomeHit.inBox(x, y, _pingBox); }

    // The PING button for a display of width w and height h: 120 x 40,
    // centred, its top 62 px above the bottom edge (y 392 to 432 on the
    // 454 px display), below the key hint and inside the circle's chord.
    public static function pingBox(w as Number, h as Number) as Array<Number> {
        return [w / 2 - 60, h - 62, 120, 40] as Array<Number>;
    }

    public function initialize(link as LinkClient) {
        View.initialize();
        _link = link;
    }

    public function onLayout(dc as Dc) as Void {
    }

    public function onShow() as Void {
    }

    public function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        var w = dc.getWidth();
        var h = dc.getHeight();
        var cx = w / 2;
        var line = h / 12;
        var y = line * 2;

        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, y, Graphics.FONT_MEDIUM, "LayerTime", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        y += line;

        var stateColor = _link.isReady() ? Graphics.COLOR_GREEN : Graphics.COLOR_YELLOW;
        dc.setColor(stateColor, Graphics.COLOR_TRANSPARENT);
        var stateText = _link.stateName();
        if (_link.nodeName.length() > 0) { stateText += "  " + _link.nodeName; }
        dc.drawText(cx, y, Graphics.FONT_SMALL, stateText, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        y += line;

        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        if (_link.newSession) {
            dc.setColor(Graphics.COLOR_ORANGE, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, y, Graphics.FONT_XTINY, "The C5 restarted. A new", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            y += line * 0.8;
            dc.drawText(cx, y, Graphics.FONT_XTINY, "Recon session has started.", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            y += line;
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        }

        var hb = _link.heartbeat >= 0 ? _link.heartbeat.toString() : "-";
        var age = _link.lastStatusMs != 0 ? ((System.getTimer() - _link.lastStatusMs) / 1000).toString() + " s ago" : "";
        dc.drawText(cx, y, Graphics.FONT_SMALL, "Heartbeat " + hb + "  " + age, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        y += line;

        var session = _link.sessionId != 0 ? _link.sessionId.format("%04X") : "-";
        dc.drawText(cx, y, Graphics.FONT_SMALL, "Session " + session + "  Link " + _link.serverVersion, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        y += line;

        var rtt = _link.lastRttMs >= 0 ? _link.lastRttMs.toString() + " ms" : "-";
        dc.drawText(cx, y, Graphics.FONT_SMALL, "Ping " + _link.acksReceived + "/" + _link.pingsSent + "  RTT " + rtt, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        y += line;

        var burst = LinkBurst.statusLines();
        for (var i = 0; i < burst.size(); i++) {
            dc.drawText(cx, y, Graphics.FONT_XTINY, burst[i], Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            y += line * 0.8;
        }
        var watch = HeartbeatWatch.statusLines();
        for (var i = 0; i < watch.size(); i++) {
            dc.drawText(cx, y, Graphics.FONT_XTINY, watch[i], Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            y += line * 0.8;
        }

        if (_link.probeReceived >= 0) {
            dc.drawText(cx, y, Graphics.FONT_SMALL, "Probe " + _link.probeReceived + " of " + _link.probeExpected + " bytes", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            y += line;
        }

        if (_link.disconnects > 0 || _link.scanRestarts > 0) {
            dc.drawText(cx, y, Graphics.FONT_XTINY, "Reconnects " + _link.disconnects + "  Scan restarts " + _link.scanRestarts, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            y += line * 0.8;
        }

        if (_link.lastError.length() > 0) {
            dc.setColor(Graphics.COLOR_RED, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, y, Graphics.FONT_XTINY, _link.lastError, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
            y += line * 0.8;
        }

        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h - line * 2, Graphics.FONT_XTINY, HeartbeatWatch.hint(), Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);

        var b = pingBox(w, h);
        _pingBox = b;
        dc.setColor(CYAN, Graphics.COLOR_TRANSPARENT);
        dc.setPenWidth(3);
        dc.drawRoundedRectangle(b[0], b[1], b[2], b[3], 10);
        dc.setPenWidth(1);
        dc.drawText(cx, b[1] + b[3] / 2, Graphics.FONT_TINY, "PING", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    public function onHide() as Void {
    }
}
