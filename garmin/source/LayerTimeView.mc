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

// The Increment 0 screen: the app name, the increment, the device's screen
// size as the watch reports it, and a count of physical button presses so
// button-first input is proven before any real UI exists.
class LayerTimeView extends WatchUi.View {

    private var _presses as Number = 0;
    private var _lastKey as String = "none";

    public function initialize() {
        View.initialize();
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

        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.24, Graphics.FONT_LARGE, WatchUi.loadResource(Rez.Strings.AppName) as String,
            Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.drawText(cx, h * 0.38, Graphics.FONT_SMALL, WatchUi.loadResource(Rez.Strings.Increment) as String,
            Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);

        var settings = System.getDeviceSettings();
        dc.drawText(cx, h * 0.52, Graphics.FONT_SMALL,
            "Screen " + settings.screenWidth + " x " + settings.screenHeight,
            Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.drawText(cx, h * 0.62, Graphics.FONT_SMALL,
            "Buttons pressed: " + _presses + " (" + _lastKey + ")",
            Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);

        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.78, Graphics.FONT_XTINY, WatchUi.loadResource(Rez.Strings.Hint) as String,
            Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    public function onHide() as Void {
    }

    // Records one physical key press. Returns the new count, so the logic is
    // testable without a display.
    public function recordKey(name as String) as Number {
        _presses += 1;
        _lastKey = name;
        WatchUi.requestUpdate();
        return _presses;
    }

    public function presses() as Number {
        return _presses;
    }
}
