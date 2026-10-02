// LayerTime - the operator interface on the wrist. Connect IQ Device App for
// the Garmin tactix 7 AMOLED (epix2pro51mm).
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

import Toybox.Attention;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.Math;
import Toybox.Position;
import Toybox.System;
import Toybox.Time;
import Toybox.Time.Gregorian;
import Toybox.WatchUi;
import Toybox.Weather;

// Which home-screen entry has the focus.
module Focus {
    const RECON = 0;
    const MAPPING = 1;
    const MESH = 2;
}

// Which home-screen control a touch lands on. Pure, so it is unit tested
// (test/HomeHitTests.mc). Each box is [x, y, w, h] in display pixels, the
// same rectangle the control is drawn in; a point is inside when
// x <= px < x + w and y <= py < y + h. The boxes do not overlap. A point in
// none of them is :none, and a tap there does nothing.
module HomeHit {
    function classify(px as Number, py as Number, recon as Array<Number>, mapping as Array<Number>,
                      mesh as Array<Number>, title as Array<Number>) as Symbol {
        if (inBox(px, py, recon)) { return :recon; }
        if (inBox(px, py, mapping)) { return :mapping; }
        if (inBox(px, py, mesh)) { return :mesh; }
        if (inBox(px, py, title)) { return :title; }
        return :none;
    }

    function inBox(px as Number, py as Number, b as Array<Number>) as Boolean {
        return px >= b[0] && px < b[0] + b[2] && py >= b[1] && py < b[1] + b[3];
    }
}

// The LayerTime home screen, to Michael's mockup of 2026-09-30: the watch
// battery as an amber segmented arc with the icon and percentage beneath,
// the LAYERTIME title, the owl with a GPS satellite icon to its left and a
// LayerWand antenna icon to its right, the clock stacked (hours ivory over
// minutes amber), a TEMP complication left and the next solar event right,
// MAPPING and MESH buttons, the amber Recon entry (mode and detection
// count), and the date.
//
// Layout (revision 2, 2026-09-30) is sized from the device's own fonts,
// measured from the SDK font files for epix2pro51mm and checked against the
// simulator: XTINY is 33 px high with 19 px capitals (VCENTER at y puts the
// capital ink at y-8..y+10); TINY 45/27; SMALL 51/31 (digit ink rows 9..40
// from the top); NUMBER_MEDIUM 118 high with 59 px digits (ink rows 28..86
// from the top). The numbers below are device pixels on the 454 px display;
// every text line has its own vertical band and fits the chord of the circle
// or ring it sits in.
//
// Every value is live or shown as unavailable: battery and clock from the
// watch; GPS from Position; outdoor temperature and sunrise/sunset from
// Toybox.Weather (the watch's cached weather, which needs the phone), with
// stale observations marked; Recon mode, event count and alert from the
// LayerTime Link Status snapshot (contracts/link.md). Only the battery arc
// is a percentage; the two complication rings are frames. Weather and GPS
// come through Env, so the preview build can show every state.
//
// The Recon entry says what the link is doing: NO LAYERWAND / SEARCHING
// before the first connection, RECONNECTING (grey, last known count) after
// a loss, LINK STALE (amber, count grey) when connected but Status is more
// than 3 s old, and the Recon mode and count when current.
class HomeView extends WatchUi.View {

    private const AMBER = 0xFFA020;
    private const IVORY = 0xF5E6C8;
    private const CYAN = 0x30D0FF;
    private const GREEN = 0x60FF50;
    private const RED = 0xFF4040;
    private const GREY = 0x8C8C8C;
    private const DIM = 0x2E2E2E;
    private const STALE_HOURS = 3;

    // Complication rings: centre y, radius, and the two centre x values.
    private const RING_Y = 190;
    private const RING_R = 62;
    private const RING_LX = 95;
    private const RING_RX = 359;

    private var _link as LinkClient;
    private var _nav as NavTracker;
    private var _owl as BitmapResource?;
    private var _scale as Float = 1.0;
    private var _lastAlertSeen as Number = 0;
    public var focus as Number = Focus.RECON;

    // Touch regions in display pixels, refreshed on each draw: [x, y, w, h].
    private var _reconBox as Array<Number> = [0, 0, 0, 0];
    private var _mappingBox as Array<Number> = [0, 0, 0, 0];
    private var _meshBox as Array<Number> = [0, 0, 0, 0];
    private var _titleBox as Array<Number> = [0, 0, 0, 0];

    public function initialize(link as LinkClient, nav as NavTracker) {
        View.initialize();
        _link = link;
        _nav = nav;
    }

    public function onLayout(dc as Dc) as Void {
        _owl = WatchUi.loadResource(Rez.Drawables.OwlHome) as BitmapResource;
        _scale = dc.getWidth() / 454.0;
    }

    // Design pixels to display pixels (1:1 on the tactix 7 AMOLED).
    private function s(v as Number) as Number { return (v * _scale + 0.5).toNumber(); }

    // Which control a tap lands on (:recon, :mapping, :mesh, :title) or
    // :none, against the boxes of the last draw, which are the rectangles
    // the controls were drawn in.
    public function hitTarget(x as Number, y as Number) as Symbol {
        return HomeHit.classify(x, y, _reconBox, _mappingBox, _meshBox, _titleBox);
    }

    public function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        var cx = dc.getWidth() / 2;
        var cy = dc.getHeight() / 2;
        drawBattery(dc, cx, cy);
        drawTitle(dc, cx);
        drawOwlAndIcons(dc, cx);
        drawClock(dc, cx);
        drawTemperature(dc, s(RING_LX), s(RING_Y), s(RING_R));
        drawSolar(dc, s(RING_RX), s(RING_Y), s(RING_R));
        drawButtons(dc);
        drawRecon(dc, cx);
        drawDate(dc, cx);
        alertIfNew();
        noticeNewSession();
    }

    // --- Battery: segmented amber arc over the top, icon and percentage
    // beneath it. The text sits at y 50 (capital ink 42..60), clear of the
    // arc's inner edge (y 36 at the text's ends).

    private function drawBattery(dc as Dc, cx as Number, cy as Number) as Void {
        var pct = System.getSystemStats().battery;
        var r = cx - s(18);
        dc.setPenWidth(s(11));
        dc.setColor(DIM, Graphics.COLOR_TRANSPARENT);
        dc.drawArc(cx, cy, r, Graphics.ARC_CLOCKWISE, 150, 30);
        var lit = (120.0 * pct / 100.0 + 0.5).toNumber();
        if (lit > 0) {
            dc.setColor(AMBER, Graphics.COLOR_TRANSPARENT);
            dc.drawArc(cx, cy, r, Graphics.ARC_CLOCKWISE, 150, 150 - lit);
        }
        dc.setPenWidth(s(3));
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
        for (var a = 150; a >= 30; a -= 4) {
            var rad = Math.toRadians(a);
            dc.drawLine((cx + (r - s(8)) * Math.cos(rad)).toNumber(), (cy - (r - s(8)) * Math.sin(rad)).toNumber(),
                        (cx + (r + s(8)) * Math.cos(rad)).toNumber(), (cy - (r + s(8)) * Math.sin(rad)).toNumber());
        }
        dc.setPenWidth(1);
        var y = s(50);
        var text = "BAT " + pct.format("%d") + "%";
        var tw = dc.getTextWidthInPixels(text, Graphics.FONT_XTINY);
        var left = cx - (tw + s(30)) / 2;
        dc.setColor(AMBER, Graphics.COLOR_TRANSPARENT);
        dc.drawRectangle(left, y - s(6), s(20), s(12));
        dc.fillRectangle(left + s(20), y - s(3), s(3), s(6));
        var fillW = (s(16) * pct / 100.0).toNumber();
        if (fillW > 0) { dc.fillRectangle(left + s(2), y - s(4), fillW, s(8)); }
        dc.drawText(left + s(30), y, Graphics.FONT_XTINY, text, Graphics.TEXT_JUSTIFY_LEFT | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    // LAYERTIME at y 76 (ink 68..86), 10 px clear of the owl below it.
    private function drawTitle(dc as Dc, cx as Number) as Void {
        var y = s(76);
        dc.setColor(IVORY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, y, Graphics.FONT_XTINY, "LAYERTIME", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        var tw = dc.getTextWidthInPixels("LAYERTIME", Graphics.FONT_XTINY);
        _titleBox = [cx - tw / 2 - s(34), y - s(14), tw + s(68), s(28)];
        dc.setColor(AMBER, Graphics.COLOR_TRANSPARENT);
        dc.setPenWidth(s(2));
        dc.drawLine(cx - tw / 2 - s(34), y + s(1), cx - tw / 2 - s(10), y + s(1));
        dc.drawLine(cx + tw / 2 + s(10), y + s(1), cx + tw / 2 + s(34), y + s(1));
        dc.setPenWidth(1);
    }

    // --- Owl (96..186) with the GPS satellite (left) and LayerWand antenna
    // (right) beside its head.

    private function drawOwlAndIcons(dc as Dc, cx as Number) as Void {
        var owl = _owl;
        var size = s(90);
        if (owl != null) { dc.drawScaledBitmap(cx - size / 2, s(96), size, size, owl); }
        Icons.satellite(dc, cx - s(67), s(118), s(14), Env.hasFix(_nav) ? GREEN : RED);
        var phase = _link.phase();
        var wandColor = phase == :ready ? GREEN : (phase == :stale ? AMBER : RED);
        Icons.antenna(dc, cx + s(67), s(118), s(14), wandColor);
    }

    // --- Clock: FONT_NUMBER_MEDIUM, hours ivory over minutes amber, the
    // digit ink 7 px apart (hours 192..251, minutes 258..317). Drawn
    // top-justified so the ink lands on those rows.

    private function drawClock(dc as Dc, cx as Number) as Void {
        var clock = System.getClockTime();
        var hour = clock.hour;
        if (!System.getDeviceSettings().is24Hour) {
            hour = hour % 12;
            if (hour == 0) { hour = 12; }
        }
        dc.setColor(IVORY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(164), Graphics.FONT_NUMBER_MEDIUM, hour.format("%02d"), Graphics.TEXT_JUSTIFY_CENTER);
        dc.setColor(AMBER, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(230), Graphics.FONT_NUMBER_MEDIUM, clock.min.format("%02d"), Graphics.TEXT_JUSTIFY_CENTER);
    }

    // --- Complications: a ring frame and four stacked bands, each with its
    // own rows: icon (centre y-37), label (XTINY, ink y-24..y-6), value
    // (FONT_SMALL, digit ink y..y+31), unit (XTINY, ink y+36..y+54).
    // The value font is FONT_SMALL because it is the largest whose widest
    // value (12:59, 95 px) fits the ring's chord at that height.

    private function ring(dc as Dc, x as Number, y as Number, r as Number, color as Number) as Void {
        dc.setPenWidth(s(7));
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        dc.drawArc(x, y, r, Graphics.ARC_CLOCKWISE, 90, 90);
        dc.setPenWidth(s(3));
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
        for (var a = 0; a < 360; a += 30) {
            var rad = Math.toRadians(a);
            dc.drawLine((x + (r - s(6)) * Math.cos(rad)).toNumber(), (y - (r - s(6)) * Math.sin(rad)).toNumber(),
                        (x + (r + s(6)) * Math.cos(rad)).toNumber(), (y - (r + s(6)) * Math.sin(rad)).toNumber());
        }
        dc.setPenWidth(1);
    }

    private function bands(dc as Dc, x as Number, y as Number, label as String, labelColor as Number,
                           value as String, valueColor as Number, unit as String, unitColor as Number) as Void {
        dc.setColor(labelColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x, y - s(16), Graphics.FONT_XTINY, label, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(valueColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x, y - s(9), Graphics.FONT_SMALL, value, Graphics.TEXT_JUSTIFY_CENTER);
        dc.setColor(unitColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x, y + s(44), Graphics.FONT_XTINY, unit, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    private function drawTemperature(dc as Dc, x as Number, y as Number, r as Number) as Void {
        var metric = System.getDeviceSettings().temperatureUnits == System.UNIT_METRIC;
        var unit = metric ? "°C" : "°F";
        var value = "--";
        var stale = false;
        var condition = null;
        var conditions = Env.weather();
        if (conditions != null) {
            var t = conditions[:temperature] as Numeric?;
            condition = conditions[:condition] as Number?;
            var observed = conditions[:observed] as Time.Moment?;
            if (observed != null && Time.now().compare(observed) > STALE_HOURS * 3600) { stale = true; }
            if (t != null) {
                var v = metric ? t : t * 9.0 / 5.0 + 32.0;
                value = v.format("%d") + "°";
            }
        }
        ring(dc, x, y, r, CYAN);
        Icons.weather(dc, x, y - s(37), s(9), condition, stale ? GREY : CYAN);
        // Stale: the label band says STALE in amber, the value is greyed,
        // and the unit stays.
        bands(dc, x, y, stale ? "STALE" : "TEMP", stale ? AMBER : CYAN,
              value, stale ? GREY : Graphics.COLOR_WHITE, unit, stale ? GREY : IVORY);
    }

    private function drawSolar(dc as Dc, x as Number, y as Number, r as Number) as Void {
        var label = "SUN";
        var value = "--";
        var unit = "";
        var rise = false;
        var known = false;
        var location = Env.location(_nav);
        if (location == null) {
            var conditions = Env.weather();
            if (conditions != null) { location = conditions[:location] as Position.Location?; }
        }
        if (location != null) {
            var now = Time.now();
            var sunrise = Weather.getSunrise(location, now);
            var sunset = Weather.getSunset(location, now);
            var next = null;
            if (sunrise != null && now.compare(sunrise) < 0) { next = sunrise; rise = true; }
            else if (sunset != null && now.compare(sunset) < 0) { next = sunset; }
            else {
                next = Weather.getSunrise(location, now.add(new Time.Duration(24 * 3600)));
                rise = true;
            }
            if (next != null) {
                known = true;
                label = rise ? "SUNRISE" : "SUNSET";
                var info = Gregorian.info(next, Time.FORMAT_SHORT);
                var hour = info.hour;
                if (System.getDeviceSettings().is24Hour) {
                    value = hour.format("%02d") + ":" + info.min.format("%02d");
                } else {
                    unit = hour >= 12 ? "PM" : "AM";
                    hour = hour % 12;
                    if (hour == 0) { hour = 12; }
                    value = hour.format("%d") + ":" + info.min.format("%02d");
                }
            }
        }
        ring(dc, x, y, r, GREEN);
        Icons.solar(dc, x, y - s(37), s(9), rise, known ? GREEN : GREY);
        bands(dc, x, y, label, known ? GREEN : GREY, value, known ? Graphics.COLOR_WHITE : GREY, unit, IVORY);
    }

    // --- MAPPING and MESH buttons: 133 x 63, so MAPPING (109 px) has 12 px
    // each side; icon band centre y+19, label ink y+35..y+54.

    private function drawButtons(dc as Dc) as Void {
        _mappingBox = [s(22), s(262), s(133), s(63)];
        _meshBox = [s(299), s(262), s(133), s(63)];
        drawButton(dc, _mappingBox, "MAPPING", focus == Focus.MAPPING, true);
        drawButton(dc, _meshBox, "MESH", focus == Focus.MESH, false);
    }

    private function drawButton(dc as Dc, b as Array<Number>, label as String, focused as Boolean, map as Boolean) as Void {
        var x = b[0]; var y = b[1]; var w = b[2]; var h = b[3];
        var c = s(12);
        var pts = [[x + c, y], [x + w - c, y], [x + w, y + c], [x + w, y + h - c], [x + w - c, y + h],
                   [x + c, y + h], [x, y + h - c], [x, y + c]];
        dc.setPenWidth(focused ? s(4) : s(2));
        dc.setColor(CYAN, Graphics.COLOR_TRANSPARENT);
        for (var i = 0; i < pts.size(); i++) {
            var p = pts[i]; var q = pts[(i + 1) % pts.size()];
            dc.drawLine(p[0], p[1], q[0], q[1]);
        }
        dc.setPenWidth(1);
        if (map) { Icons.map(dc, x + w / 2, y + s(19), s(10), CYAN); }
        else { Icons.mesh(dc, x + w / 2, y + s(19), s(10), CYAN); }
        dc.drawText(x + w / 2, y + s(43), Graphics.FONT_XTINY, label, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    // --- Recon entry: two lines inside chevron brackets, border 326..393.
    // Mode in XTINY (ink 334..353), count in FONT_TINY (ink 359..386);
    // the whole bracket is the touch target.

    private function drawRecon(dc as Dc, cx as Number) as Void {
        var phase = _link.phase();
        var mode = "NO LAYERWAND";
        var color = GREY;
        var count = phase == :failed ? "LINK FAILED" : "SEARCHING";
        var countColor = GREY;
        var alert = false;
        if (phase == :ready || phase == :stale || phase == :reconnecting) {
            count = ReconNames.countText(_link.eventCount);
            if (phase == :reconnecting) {
                mode = "RECONNECTING";
            } else if (phase == :stale) {
                mode = "LINK STALE";
                color = AMBER;
            } else {
                mode = modeText();
                color = AMBER;
                countColor = AMBER;
            }
            alert = phase != :reconnecting && (_link.flags & Link.FLAG_ALERT_PENDING) != 0;
        }
        if (alert) { mode = "ALERT  " + mode; color = RED; }
        var top = s(326);
        var bottom = s(393);
        // The bracket's bottom corners must stay inside the circle.
        var r = dc.getWidth() / 2;
        var dy = bottom - dc.getHeight() / 2;
        var maxW = (2.0 * Math.sqrt(r * r - dy * dy)).toNumber() - s(4);
        var countW = dc.getTextWidthInPixels(count, Graphics.FONT_TINY);
        var modeW = dc.getTextWidthInPixels(mode, Graphics.FONT_XTINY);
        if (alert && modeW + s(44) > maxW) {
            mode = "ALERT";
            modeW = dc.getTextWidthInPixels(mode, Graphics.FONT_XTINY);
        }
        var w = (modeW > countW ? modeW : countW) + s(44);
        if (w < s(200)) { w = s(200); }
        if (w > maxW) { w = maxW; }
        _reconBox = [cx - w / 2, top, w, bottom - top];
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(342), Graphics.FONT_XTINY, mode, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(countColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(371), Graphics.FONT_TINY, count, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setPenWidth(focus == Focus.RECON ? s(4) : s(2));
        dc.setColor(alert ? RED : AMBER, Graphics.COLOR_TRANSPARENT);
        var mid = (top + bottom) / 2;
        var lx = cx - w / 2;
        var rx = cx + w / 2;
        var k = s(14);
        dc.drawLine(lx + k, top, lx, mid); dc.drawLine(lx, mid, lx + k, bottom);
        dc.drawLine(rx - k, top, rx, mid); dc.drawLine(rx, mid, rx - k, bottom);
        dc.drawLine(lx + k, top, lx + s(52), top); dc.drawLine(rx - s(52), top, rx - k, top);
        dc.drawLine(lx + k, bottom, lx + s(52), bottom); dc.drawLine(rx - s(52), bottom, rx - k, bottom);
        dc.setPenWidth(1);
    }

    // The Recon mode line, from Status flags and the selection.
    private function modeText() as String {
        return ReconNames.modeText(_link.flags, _link.selected);
    }

    // Date at y 409 (ink 401..419), inside the circle's chord there.
    private function drawDate(dc as Dc, cx as Number) as Void {
        var info = Gregorian.info(Time.now(), Time.FORMAT_MEDIUM);
        var dow = info.day_of_week as String;
        var month = info.month as String;
        var text = dow.toUpper() + " • " + month.toUpper() + " " + info.day.format("%d");
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(409), Graphics.FONT_XTINY, text, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    // One short vibration when Status reports a new alert event id; sleep
    // mode (flags bit 4) suppresses it, as on the Node.
    private function alertIfNew() as Void {
        var id = _link.lastAlertEventId;
        if (id == 0 || id == _lastAlertSeen) { return; }
        _lastAlertSeen = id;
        if ((_link.flags & Link.FLAG_SLEEP_MODE) != 0) { return; }
        if (Attention has :vibrate) {
            Attention.vibrate([new Attention.VibeProfile(50, 400)]);
        }
    }

    // A new LayerWand session is announced once as a native toast, not on
    // the face, and then acknowledged. The Recon page keeps the session id.
    private function noticeNewSession() as Void {
        if (!_link.newSession) { return; }
        _link.acknowledgeNewSession();
        if (WatchUi has :showToast) {
            WatchUi.showToast("New LayerWand session", null);
        }
    }
}
