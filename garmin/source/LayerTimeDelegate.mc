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

// Physical-button-first input. START sends a PING; BACK is left to the
// system so it exits the app, which is the intended way to leave the
// mission interface. A new-session notice is dismissed by any button. On
// the linktest build only, UP starts or stops the 100-PING burst
// (LinkBurst.mc) and DOWN starts the 10-minute Status measurement
// (HeartbeatWatch.mc; a second DOWN during a run is ignored); the release
// build's LinkBurst.toggle() and HeartbeatWatch.start() do nothing.
class LayerTimeDelegate extends WatchUi.BehaviorDelegate {

    private var _link as LinkClient;

    public function initialize(link as LinkClient) {
        BehaviorDelegate.initialize();
        _link = link;
    }

    public function onSelect() as Boolean {
        if (_link.newSession) {
            _link.acknowledgeNewSession();
            return true;
        }
        _link.ping();
        WatchUi.requestUpdate();
        return true;
    }

    public function onNextPage() as Boolean {
        if (_link.newSession) {
            _link.acknowledgeNewSession();
            return true;
        }
        HeartbeatWatch.start(_link);
        WatchUi.requestUpdate();
        return true;
    }

    public function onPreviousPage() as Boolean {
        if (_link.newSession) {
            _link.acknowledgeNewSession();
            return true;
        }
        LinkBurst.toggle(_link);
        WatchUi.requestUpdate();
        return true;
    }

    public function onMenu() as Boolean {
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
