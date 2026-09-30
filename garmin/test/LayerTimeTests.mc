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
import Toybox.Test;
import Toybox.WatchUi;

// Run No Evil unit tests for the app shell. Compiled only with --unit-test
// and run in the simulator (see README.md).
module LayerTimeTests {

    (:test)
    function keyNamesAreTheLabelsOnTheWatch(logger as Logger) as Boolean {
        var ok = LayerTimeDelegate.keyName(WatchUi.KEY_ENTER).equals("START");
        ok = ok && LayerTimeDelegate.keyName(WatchUi.KEY_UP).equals("UP");
        ok = ok && LayerTimeDelegate.keyName(WatchUi.KEY_DOWN).equals("DOWN");
        ok = ok && LayerTimeDelegate.keyName(WatchUi.KEY_MENU).equals("MENU");
        logger.debug("key names checked");
        return ok;
    }
}
