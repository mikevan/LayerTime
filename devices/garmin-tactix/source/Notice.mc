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
import Toybox.WatchUi;

// A full-screen notice: an amber title and a sentence wrapped to fit the
// round face, on black. Any of BACK, START, or a tap dismisses it. Used for
// the LayerWand's no-SD-card warning (Controls.showNoSdCard).
class NoticeView extends WatchUi.View {

    private const AMBER = 0xFFA020;
    private var _title as String;
    private var _text as String;

    public function initialize(title as String, text as String) {
        View.initialize();
        _title = title;
        _text = text;
    }

    public function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        var w = dc.getWidth();
        var h = dc.getHeight();
        var c = Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER;
        dc.setColor(AMBER, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w / 2, h * 22 / 100, Graphics.FONT_SMALL, _title, c);
        // The middle of a round face: about 76 percent of the width, under the
        // title. The whole sentence must show: the largest font that fits it
        // without cutting wins (FONT_TINY cut the warning short on the
        // epix2pro51mm, 2026-10-03). FONT_XTINY, cut if it must, is the floor.
        var boxW = w * 76 / 100;
        var boxH = h * 52 / 100;
        var font = Graphics.FONT_TINY;
        var fitted = Graphics.fitTextToArea(_text, font, boxW, boxH, false);
        if (fitted == null) {
            font = Graphics.FONT_XTINY;
            fitted = Graphics.fitTextToArea(_text, font, boxW, boxH, false);
        }
        if (fitted == null) { fitted = Graphics.fitTextToArea(_text, font, boxW, boxH, true); }
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w / 2, h * 58 / 100, font, fitted != null ? fitted : "", c);
    }
}

// On the tactix 7 AMOLED both START and a tap are the Select behavior
// (HomeDelegate.mc). Returning true from onSelect suppresses the matching
// input callback, so one press pops this view once and never the page
// under it. No onKey handler, for the same reason.
class NoticeDelegate extends WatchUi.BehaviorDelegate {

    public function initialize() { BehaviorDelegate.initialize(); }

    public function onSelect() as Boolean { return close(); }

    public function onBack() as Boolean { return close(); }

    private function close() as Boolean {
        WatchUi.popView(WatchUi.SLIDE_DOWN);
        return true;
    }
}
