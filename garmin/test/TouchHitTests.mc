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

// Touch routing on the Recon page and the Link diagnostics page, with the
// boxes those pages draw on the 454 px display. Recon rows (ReconView
// drawRow): five 44 px rows from y 138, each as wide as the circle's chord
// at its outer edge less 10 px each side; the MENU  CONTROLS line
// (XTINY, 204 px of text) with 10 px each side, y 398 to 430. The PING
// button: LayerTimeView.pingBox(454, 454).
module TouchHitTests {

    const ROWS = [
        [29, 138, 396, 44, 0], [15, 182, 424, 44, 1], [15, 226, 424, 44, 2],
        [28, 270, 398, 44, 3], [52, 314, 350, 44, 4]
    ] as Array<Array<Number> >;
    const MENU = [115, 398, 224, 32] as Array<Number>;

    function recon(logger as Logger, x as Number, y as Number, want as Number) as Boolean {
        var got = ReconHit.classify(x, y, ROWS, MENU);
        if (got == want) { return true; }
        logger.error("Recon tap (" + x + ", " + y + "): expected " + want + " got " + got);
        return false;
    }

    (:test)
    function reconRowsOpenTheirDetectionAndControlsOpensControls(logger as Logger) as Boolean {
        var ok = recon(logger, 227, 160, 0);
        ok = recon(logger, 227, 204, 1) && ok;
        ok = recon(logger, 227, 248, 2) && ok;
        ok = recon(logger, 227, 292, 3) && ok;
        ok = recon(logger, 227, 336, 4) && ok;
        ok = recon(logger, 60, 330, 4) && ok;           // left end of the last row
        ok = recon(logger, 227, 414, -2) && ok;         // MENU  CONTROLS
        ok = recon(logger, 115, 398, -2) && ok;
        ok = recon(logger, 338, 429, -2) && ok;
        return ok;
    }

    (:test)
    function reconEmptySpaceHitsNothing(logger as Logger) as Boolean {
        var ok = recon(logger, 227, 52, -1);            // RECON title
        ok = recon(logger, 227, 84, -1) && ok;          // mode line
        ok = recon(logger, 227, 114, -1) && ok;         // count line
        ok = recon(logger, 227, 137, -1) && ok;         // one above the first row
        ok = recon(logger, 227, 358, -1) && ok;         // one below the last row
        ok = recon(logger, 227, 388, -1) && ok;         // position line
        ok = recon(logger, 40, 336, -1) && ok;          // left of the last row
        ok = recon(logger, 227, 430, -1) && ok;         // one below MENU  CONTROLS
        // With fewer detections, a tap where a row would be is empty space.
        var two = [ROWS[0], ROWS[1]] as Array<Array<Number> >;
        var got = ReconHit.classify(227, 292, two, MENU);
        if (got != -1) { logger.error("undrawn row answered " + got); ok = false; }
        return ok;
    }

    (:test)
    function onlyThePingButtonPings(logger as Logger) as Boolean {
        var b = LayerTimeView.pingBox(454, 454);
        var ok = b[0] == 167 && b[1] == 392 && b[2] == 120 && b[3] == 40;
        ok = ok && HomeHit.inBox(227, 412, b) && HomeHit.inBox(167, 392, b) && HomeHit.inBox(286, 431, b);
        ok = ok && !HomeHit.inBox(166, 412, b) && !HomeHit.inBox(287, 412, b) && !HomeHit.inBox(227, 391, b) && !HomeHit.inBox(227, 432, b);
        ok = ok && !HomeHit.inBox(227, 227, b) && !HomeHit.inBox(227, 74, b) && !HomeHit.inBox(227, 380, b);
        // Inside the round display: both bottom corners within radius 227.
        var dx = 60;
        var dy = 432 - 227;
        ok = ok && dx * dx + dy * dy < 227 * 227;
        if (!ok) { logger.error("PING button bounds broke"); }
        return ok;
    }
}
