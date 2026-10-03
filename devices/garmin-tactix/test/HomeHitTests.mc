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

// Home-screen touch routing: which control a tap lands on. The boxes are the
// ones HomeView draws on the 454 px display: MAPPING [22, 262, 133, 63] and
// MESH [299, 262, 133, 63] (drawButtons); the Recon bracket from y 326 to
// 393, as wide as its widest line plus 44 (drawRecon; "12 DETECTIONS" in
// FONT_TINY is 241 px, so 285 wide from x 85); the LAYERTIME title band,
// 128 px of text plus 34 px each side, y 62 to 90 (drawTitle).
module HomeHitTests {

    const RECON = [85, 326, 285, 67] as Array<Number>;
    const MAPPING = [22, 262, 133, 63] as Array<Number>;
    const MESH = [299, 262, 133, 63] as Array<Number>;
    const TITLE = [129, 62, 196, 28] as Array<Number>;

    function hit(x as Number, y as Number) as Symbol {
        return HomeHit.classify(x, y, RECON, MAPPING, MESH, TITLE);
    }

    function expect(logger as Logger, x as Number, y as Number, want as Symbol) as Boolean {
        var got = hit(x, y);
        if (got == want) { return true; }
        logger.error("tap (" + x + ", " + y + "): expected " + want + " got " + got);
        return false;
    }

    (:test)
    function eachControlIsHitAtItsCentreAndCorners(logger as Logger) as Boolean {
        var ok = expect(logger, 227, 359, :recon);     // Recon entry centre
        ok = expect(logger, 85, 326, :recon) && ok;     // its top-left pixel
        ok = expect(logger, 369, 392, :recon) && ok;    // its bottom-right pixel
        ok = expect(logger, 88, 293, :mapping) && ok;   // MAPPING centre
        ok = expect(logger, 22, 262, :mapping) && ok;
        ok = expect(logger, 154, 324, :mapping) && ok;
        ok = expect(logger, 365, 293, :mesh) && ok;     // MESH centre
        ok = expect(logger, 299, 262, :mesh) && ok;
        ok = expect(logger, 431, 324, :mesh) && ok;
        ok = expect(logger, 227, 76, :title) && ok;     // LAYERTIME
        ok = expect(logger, 129, 62, :title) && ok;
        ok = expect(logger, 324, 89, :title) && ok;
        return ok;
    }

    (:test)
    function tapsOutsideEveryControlHitNothing(logger as Logger) as Boolean {
        var ok = expect(logger, 227, 20, :none);        // battery arc
        ok = expect(logger, 227, 50, :none) && ok;      // BAT n%
        ok = expect(logger, 227, 140, :none) && ok;     // owl
        ok = expect(logger, 227, 220, :none) && ok;     // clock
        ok = expect(logger, 95, 190, :none) && ok;      // temperature ring
        ok = expect(logger, 359, 190, :none) && ok;     // sunrise ring
        ok = expect(logger, 227, 293, :none) && ok;     // between MAPPING and MESH
        ok = expect(logger, 227, 409, :none) && ok;     // date
        ok = expect(logger, 5, 5, :none) && ok;         // corner
        return ok;
    }

    (:test)
    function edgesAreExclusiveSoNoTwoControlsShareAPixel(logger as Logger) as Boolean {
        var ok = expect(logger, 155, 293, :none);       // one past MAPPING's right edge
        ok = expect(logger, 298, 293, :none) && ok;     // one before MESH
        ok = expect(logger, 88, 325, :none) && ok;      // one below MAPPING, one above Recon
        ok = expect(logger, 370, 359, :none) && ok;     // one past Recon's right edge
        ok = expect(logger, 227, 393, :none) && ok;     // one below Recon
        ok = expect(logger, 227, 90, :none) && ok;      // one below the title
        ok = expect(logger, 128, 76, :none) && ok;      // one left of the title
        // No two boxes intersect, so no pixel belongs to two controls.
        var boxes = [RECON, MAPPING, MESH, TITLE];
        for (var i = 0; i < boxes.size(); i++) {
            for (var j = i + 1; j < boxes.size(); j++) {
                var a = boxes[i] as Array<Number>;
                var b = boxes[j] as Array<Number>;
                var apart = a[0] + a[2] <= b[0] || b[0] + b[2] <= a[0] || a[1] + a[3] <= b[1] || b[1] + b[3] <= a[1];
                if (!apart) { logger.error("boxes " + i + " and " + j + " overlap"); ok = false; }
            }
        }
        return ok;
    }
}
