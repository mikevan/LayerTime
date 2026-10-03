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
import Toybox.Weather;

// Where the watch app gets what it did not compute itself: the LayerWand
// link's lifecycle, the watch's weather, and its GPS. The release and test
// builds use the (:nopreview) functions, which read the real sources. The
// preview build (preview.jungle) excludes them and uses the (:preview)
// versions in Preview.mc, which serve fixed scenarios so every home-screen
// state can be shown in the simulator without a LayerWand, weather, or GPS.
module Env {

    (:nopreview)
    function startLink(link as LinkClient) as Void { link.start(); }

    (:nopreview)
    function stopLink(link as LinkClient) as Void { link.stop(); }

    // The watch's cached current conditions as {:temperature (Float?, °C),
    // :condition (Number?), :observed (Moment?), :location (Location?)}, or
    // null when the watch has none.
    (:nopreview)
    function weather() as Dictionary? {
        var c = Weather.getCurrentConditions();
        if (c == null) { return null; }
        return {
            :temperature => c.temperature,
            :condition => c.condition,
            :observed => c.observationTime,
            :location => c.observationLocationPosition
        };
    }

    (:nopreview)
    function hasFix(nav as NavTracker) as Boolean { return nav.hasFix(); }

    (:nopreview)
    function location(nav as NavTracker) as Position.Location? { return nav.lastLocation(); }

    // The link phase a preview scenario forces; always null outside previews.
    (:nopreview)
    function forcedPhase() as Symbol? { return null; }

    // A tap on the LAYERTIME title: nothing outside previews (returns false
    // so the tap falls through).
    (:nopreview)
    function titleTapped(link as LinkClient) as Boolean { return false; }
}
