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
import Toybox.Math;
import Toybox.System;
import Toybox.WatchUi;

// The Recon page: the LayerWand's detections, newest first, from the
// ReconMirror (GET_CHANGED). Each row is the detection type, its RSSI and
// how many times it was seen; the row of the event that raised the pending
// alert is red. The two lines above the list are the Recon mode and the
// link state, in the same words as the home screen; the count line turns
// amber and reads "<n> DETECTIONS  GAP" when detections were missed.
//
// Keys: UP and DOWN move the highlight, START opens the highlighted
// detection, MENU opens the Controls, BACK returns home. Touch: a tap on a
// row opens that detection; a tap on the MENU  CONTROLS line at the bottom
// opens the Controls; a tap anywhere else does nothing. START is handled in
// onKey, not onSelect, because on this watch a screen tap is also the
// Select behavior (see HomeDelegate.mc).
//
// Layout on the 454 px display, from the device fonts (HomeView.mc): title
// and mode lines at y 52 and 84, the link line at 114, five 44 px rows from
// y 138 to 358, each inside the circle's chord at its outer edge, the
// position at 388 and the MENU hint at 414.
class ReconView extends WatchUi.View {

    private const AMBER = 0xFFA020;
    private const IVORY = 0xF5E6C8;
    private const CYAN = 0x30D0FF;
    private const RED = 0xFF4040;
    private const GREY = 0x8C8C8C;
    private const ROWS = 5;
    private const ROW_H = 44;
    private const LIST_TOP = 138;

    private var _link as LinkClient;
    private var _scale as Float = 1.0;
    public var focus as Number = 0;
    private var _first as Number = 0;
    // Row boxes of the last draw, [x, y, w, h, index], and the MENU hint.
    private var _rowBoxes as Array<Array<Number> > = [] as Array<Array<Number> >;
    private var _menuBox as Array<Number> = [0, 0, 0, 0];

    public function initialize(link as LinkClient) {
        View.initialize();
        _link = link;
    }

    public function onLayout(dc as Dc) as Void {
        _scale = dc.getWidth() / 454.0;
    }

    private function s(v as Number) as Number { return (v * _scale + 0.5).toNumber(); }

    public function events() as Array<Dictionary> { return _link.mirror.events(); }

    public function move(delta as Number) as Void {
        var n = _link.mirror.count();
        if (n == 0) { focus = 0; return; }
        focus = (focus + delta + n) % n;
        WatchUi.requestUpdate();
    }

    // The event a tap lands on, -2 for the MENU  CONTROLS line, or -1,
    // against the boxes of the last draw.
    public function hitTest(x as Number, y as Number) as Number {
        return ReconHit.classify(x, y, _rowBoxes, _menuBox);
    }

    public function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        var w = dc.getWidth();
        var cx = w / 2;
        var r = w / 2;
        var phase = _link.phase();
        var alert = (phase == :ready || phase == :stale) && (_link.flags & Link.FLAG_ALERT_PENDING) != 0;

        dc.setColor(IVORY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(52), Graphics.FONT_XTINY, "RECON", Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);

        var mode = ReconNames.modeText(_link.flags, _link.selected);
        var modeColor = phase == :ready ? AMBER : GREY;
        if (alert) { mode = "ALERT  " + mode; modeColor = RED; }
        if (phase == :searching || phase == :failed) { mode = "NO LAYERWAND"; }
        dc.setColor(modeColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(84), Graphics.FONT_XTINY, mode, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);

        var linkText = ReconNames.countText(_link.eventCount);
        var linkColor = Graphics.COLOR_WHITE;
        // Detections were dropped on LayerWand before the watch fetched them
        // (END gap). Shown until Clear succeeds or a new session starts.
        if (_link.mirror.gaps > 0) { linkText += "  GAP"; linkColor = AMBER; }
        // No SD card log on the LayerWand: its events live only in memory.
        if ((_link.flags & Link.FLAG_NO_SD_LOG) != 0) { linkText += "  NO SD"; linkColor = AMBER; }
        // An Increment 1 Link-only LayerWand runs no Recon.
        if (phase == :ready && !_link.reconAvailable) { linkText = "LINK-ONLY LAYERWAND"; linkColor = GREY; }
        if (phase == :stale) { linkText = "LINK STALE"; linkColor = AMBER; }
        else if (phase == :reconnecting) { linkText = "RECONNECTING"; linkColor = GREY; }
        else if (phase == :searching) { linkText = "SEARCHING"; linkColor = GREY; }
        else if (phase == :failed) { linkText = "LINK FAILED"; linkColor = RED; }
        dc.setColor(linkColor, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(114), Graphics.FONT_XTINY, linkText, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);

        var list = events();
        var n = list.size();
        _rowBoxes = [] as Array<Array<Number> >;
        if (n == 0) {
            var empty = (phase == :ready && !_link.mirror.synced) ? "LOADING..." : "NO DETECTIONS";
            dc.setColor(GREY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, s(LIST_TOP + ROWS * ROW_H / 2), Graphics.FONT_TINY, empty,
                        Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        } else {
            if (focus >= n) { focus = n - 1; }
            if (focus < _first) { _first = focus; }
            if (focus >= _first + ROWS) { _first = focus - ROWS + 1; }
            if (_first > n - 1) { _first = 0; }
            var now = System.getTimer();
            for (var i = 0; i < ROWS && _first + i < n; i++) {
                drawRow(dc, cx, r, s(LIST_TOP + i * ROW_H), s(ROW_H), list[_first + i], _first + i, alert, now);
            }
            var last = _first + ROWS < n ? _first + ROWS : n;
            dc.setColor(GREY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, s(388), Graphics.FONT_XTINY, (_first + 1) + "-" + last + " / " + n,
                        Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        }

        var hint = "MENU  CONTROLS";
        var hw = dc.getTextWidthInPixels(hint, Graphics.FONT_XTINY);
        _menuBox = [cx - hw / 2 - s(10), s(398), hw + s(20), s(32)];
        dc.setColor(CYAN, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(414), Graphics.FONT_XTINY, hint, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    private function drawRow(dc as Dc, cx as Number, r as Number, top as Number, h as Number, e as Dictionary,
                             index as Number, alert as Boolean, now as Number) as Void {
        // The row's outer edge sets its width: the chord there, less a margin.
        var cy = dc.getHeight() / 2;
        var edge = (top + h / 2 < cy) ? top : top + h;
        var dy = edge - cy;
        var half = Math.sqrt(r * r - dy * dy).toNumber() - s(10);
        var left = cx - half;
        var width = half * 2;
        var mid = top + h / 2;
        _rowBoxes.add([left, top, width, h, index]);

        if (index == focus) {
            dc.setPenWidth(s(3));
            dc.setColor(CYAN, Graphics.COLOR_TRANSPARENT);
            dc.drawRoundedRectangle(left, top + s(3), width, h - s(6), s(10));
            dc.setPenWidth(1);
        }

        var id = e[:eventId] as Number;
        var alerting = alert && id == _link.lastAlertEventId;
        var name = ReconNames.shortName(e[:detector] as Number);
        var count = e[:count] as Number;
        var right = (e[:rssi] as Number).format("%d") + "  x" + (count > 999 ? "999+" : count.format("%d"));
        var rightW = dc.getTextWidthInPixels(right, Graphics.FONT_XTINY);
        var font = Graphics.FONT_TINY;
        if (dc.getTextWidthInPixels(name, font) + rightW + s(40) > width) { font = Graphics.FONT_XTINY; }

        dc.setColor(alerting ? RED : Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(left + s(14), mid, font, name, Graphics.TEXT_JUSTIFY_LEFT | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.setColor(IVORY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(left + width - s(14), mid, Graphics.FONT_XTINY, right, Graphics.TEXT_JUSTIFY_RIGHT | Graphics.TEXT_JUSTIFY_VCENTER);
    }
}

// Which Recon-page control a touch lands on. Pure, so it is unit tested
// (test/TouchHitTests.mc). rows holds [x, y, w, h, index] per drawn row;
// menu is [x, y, w, h] of the MENU  CONTROLS line. Returns the row's event
// index, -2 for the Controls line, or -1 for anything else.
module ReconHit {
    function classify(px as Number, py as Number, rows as Array<Array<Number> >, menu as Array<Number>) as Number {
        for (var i = 0; i < rows.size(); i++) {
            var b = rows[i];
            if (HomeHit.inBox(px, py, b)) { return b[4]; }
        }
        if (HomeHit.inBox(px, py, menu)) { return -2; }
        return -1;
    }
}

class ReconDelegate extends WatchUi.BehaviorDelegate {

    private var _link as LinkClient;
    private var _view as ReconView;

    public function initialize(link as LinkClient, view as ReconView) {
        BehaviorDelegate.initialize();
        _link = link;
        _view = view;
    }

    private function open(index as Number) as Void {
        var list = _view.events();
        if (index < 0 || index >= list.size()) { return; }
        var id = list[index][:eventId] as Number;
        var v = new EventView(_link, id);
        WatchUi.pushView(v, new EventDelegate(), WatchUi.SLIDE_LEFT);
    }

    // A tap goes on to onTap, START to onKey.
    public function onSelect() as Boolean { return false; }

    public function onKey(evt as KeyEvent) as Boolean {
        if (evt.getKey() != WatchUi.KEY_ENTER) { return false; }
        open(_view.focus);
        return true;
    }
    public function onNextPage() as Boolean { _view.move(1); return true; }
    public function onPreviousPage() as Boolean { _view.move(-1); return true; }
    public function onMenu() as Boolean { Controls.open(_link); return true; }

    public function onTap(evt as ClickEvent) as Boolean {
        var xy = evt.getCoordinates();
        var hit = _view.hitTest(xy[0], xy[1]);
        if (hit == -2) { Controls.open(_link); return true; }
        if (hit < 0) { return true; } // empty space: consumed, nothing happens
        _view.focus = hit;
        open(hit);
        return true;
    }
}

// One detection in full: type, confidence, RSSI, sightings, age, radio,
// channel and band, the observed source identifier and detail text (fetched
// with GET_TEXT when the page opens), and the event id.
class EventView extends WatchUi.View {

    private const AMBER = 0xFFA020;
    private const IVORY = 0xF5E6C8;
    private const GREY = 0x8C8C8C;

    private var _link as LinkClient;
    private var _id as Number;
    private var _scale as Float = 1.0;

    public function initialize(link as LinkClient, eventId as Number) {
        View.initialize();
        _link = link;
        _id = eventId;
    }

    public function onLayout(dc as Dc) as Void { _scale = dc.getWidth() / 454.0; }

    private function s(v as Number) as Number { return (v * _scale + 0.5).toNumber(); }

    public function onShow() as Void {
        _link.textObserver = method(:onText);
        fetchMissing();
    }

    public function onHide() as Void { _link.textObserver = null; }

    public function onText(eventId as Number) as Void {
        if (eventId == _id) { fetchMissing(); }
        WatchUi.requestUpdate();
    }

    // Source id first, then detail; one GET_TEXT at a time.
    private function fetchMissing() as Void {
        var e = _link.mirror.get(_id);
        if (e == null) { return; }
        var flags = e[:flags] as Number;
        if ((flags & Link.SUMMARY_HAS_SOURCE_ID) != 0 && e[:sourceId] == null) {
            _link.fetchText(_id, Link.TEXT_FIELD_SOURCE_ID);
        } else if ((flags & Link.SUMMARY_HAS_DETAIL) != 0 && e[:detail] == null) {
            _link.fetchText(_id, Link.TEXT_FIELD_DETAIL);
        }
    }

    public function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        var cx = dc.getWidth() / 2;
        var c = Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER;
        var e = _link.mirror.get(_id);
        if (e == null) {
            dc.setColor(GREY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, dc.getHeight() / 2, Graphics.FONT_TINY, "NO LONGER HELD", c);
            return;
        }
        var flags = e[:flags] as Number;
        dc.setColor(AMBER, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(84), Graphics.FONT_TINY, ReconNames.displayName(e[:detector] as Number), c);
        dc.setColor(IVORY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(122), Graphics.FONT_XTINY, ReconNames.confidenceName(e[:confidence] as Number) + " CONFIDENCE", c);
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(166), Graphics.FONT_MEDIUM, (e[:rssi] as Number).format("%d") + " dBm", c);

        var count = e[:count] as Number;
        var seen = "SEEN " + (count >= 65535 ? "65535+" : count.format("%d")) + (count == 1 ? " TIME  " : " TIMES  ") +
                   age(ReconMirror.ageSeconds(e, System.getTimer())) + " AGO";
        dc.drawText(cx, s(208), Graphics.FONT_XTINY, seen, c);

        var kind = e[:sourceKind] as Number;
        var radio = kind == 1 ? "WI-FI" : (kind == 2 ? "BLUETOOTH LE" : (kind == 3 ? "802.15.4" : "RADIO ?"));
        var ch = e[:channel] as Number;
        if (ch != 0) { radio += "  CH " + ch.format("%d"); }
        var band = e[:band] as Number;
        if (band == 1) { radio += "  2.4 GHZ"; } else if (band == 2) { radio += "  5 GHZ"; }
        dc.setColor(IVORY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(242), Graphics.FONT_XTINY, radio, c);

        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(282), Graphics.FONT_XTINY, text(e, :sourceId, flags & Link.SUMMARY_HAS_SOURCE_ID), c);
        var lines = wrap(dc, text(e, :detail, flags & Link.SUMMARY_HAS_DETAIL), s(360));
        for (var i = 0; i < lines.size() && i < 2; i++) {
            dc.drawText(cx, s(316 + 32 * i), Graphics.FONT_XTINY, lines[i], c);
        }
        dc.setColor(GREY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, s(396), Graphics.FONT_XTINY, "EVENT " + _id.format("%d"), c);
    }

    private function text(e as Dictionary, key as Symbol, present as Number) as String {
        if (present == 0) { return "-"; }
        var t = e[key] as String?;
        if (t != null) { return t; }
        return _link.isReady() ? "FETCHING..." : "NOT FETCHED";
    }

    private static function age(sec as Number) as String {
        if (sec < 60) { return sec.format("%d") + " S"; }
        if (sec < 3600) { return (sec / 60).format("%d") + " MIN"; }
        return (sec / 3600).format("%d") + " H";
    }

    // Up to two lines no wider than maxW, broken at spaces where possible.
    private static function wrap(dc as Dc, t as String, maxW as Number) as Array<String> {
        if (dc.getTextWidthInPixels(t, Graphics.FONT_XTINY) <= maxW) { return [t] as Array<String>; }
        var chars = t.toCharArray();
        var best = -1;
        var cut = chars.size();
        for (var i = 1; i < chars.size(); i++) {
            var head = t.substring(0, i) as String;
            if (dc.getTextWidthInPixels(head, Graphics.FONT_XTINY) > maxW) { cut = i - 1; break; }
            if (chars[i] == ' ') { best = i; }
        }
        var split = best > 0 ? best : cut;
        var first = t.substring(0, split) as String;
        var rest = t.substring(best > 0 ? split + 1 : split, t.length()) as String;
        return [first, rest] as Array<String>;
    }
}

class EventDelegate extends WatchUi.BehaviorDelegate {
    public function initialize() { BehaviorDelegate.initialize(); }
}
