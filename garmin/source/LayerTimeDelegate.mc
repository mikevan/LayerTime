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
import Toybox.WatchUi;

// Physical-button-first input. BACK is left to the system so it exits the
// app, which is the intended Slice 1 way to leave the mission interface.
class LayerTimeDelegate extends WatchUi.BehaviorDelegate {

    private var _view as LayerTimeView;

    public function initialize(view as LayerTimeView) {
        BehaviorDelegate.initialize();
        _view = view;
    }

    public function onSelect() as Boolean {
        _view.recordKey("START");
        return true;
    }

    public function onNextPage() as Boolean {
        _view.recordKey("DOWN");
        return true;
    }

    public function onPreviousPage() as Boolean {
        _view.recordKey("UP");
        return true;
    }

    public function onMenu() as Boolean {
        _view.recordKey("MENU");
        return true;
    }

    public function onKey(keyEvent as KeyEvent) as Boolean {
        _view.recordKey(keyName(keyEvent.getKey()));
        return true;
    }

    // Names the keys LayerTime will bind in Increment 4. Pure function; tested.
    public static function keyName(key as Key) as String {
        if (key == WatchUi.KEY_ENTER) { return "START"; }
        if (key == WatchUi.KEY_UP) { return "UP"; }
        if (key == WatchUi.KEY_DOWN) { return "DOWN"; }
        if (key == WatchUi.KEY_MENU) { return "MENU"; }
        if (key == WatchUi.KEY_LAP) { return "BACK"; }
        if (key == WatchUi.KEY_LIGHT) { return "LIGHT"; }
        return "key " + key;
    }
}
