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

// Slice 1 Increment 1: the app owns the LayerTime Link client for as long
// as it runs (foreground Device App by decision; plan section 0B).

import Toybox.Application;
import Toybox.Lang;
import Toybox.WatchUi;

class LayerTimeApp extends Application.AppBase {

    private var _link as LinkClient?;

    public function initialize() {
        AppBase.initialize();
    }

    public function onStart(state as Dictionary?) as Void {
        _link = new LinkClient();
        _link.start();
    }

    public function onStop(state as Dictionary?) as Void {
        if (_link != null) {
            _link.stop();
            _link = null;
        }
    }

    public function getInitialView() as [Views] or [Views, InputDelegates] {
        var link = _link as LinkClient;
        var view = new $.LayerTimeView(link);
        return [view, new $.LayerTimeDelegate(link)];
    }
}
