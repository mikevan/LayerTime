// LayerTime - the operator interface on the wrist. Connect IQ Device App for
// the Garmin tactix 7 AMOLED (epix2pro51mm).
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

// ReconTarget names for the wrist, copied from
// contracts/vectors/enums.json (reconTargetShortNames and
// reconTargetDisplayNames), indexed by the ReconTarget value. Keep in step
// with the JSON; the C++ side reads the same table through core.
module ReconNames {
    const SHORT = [
        "STOPPED", "ALL", "TRACKERS", "SURVEIL", "INTRUSION", "DEAUTH", "PWNAGOTCHI", "MULTISSID",
        "FLOCK", "PINEAPPLE", "AIRTAG", "FLIPPER", "META", "AXON", "TILE", "SMARTTAG", "GOOGLE TAG",
        "EARLY WARN"
    ] as Array<String>;
    const DISPLAY = [
        "STOPPED", "ALL", "TRACKERS", "COUNTER-SURVEIL", "COUNTER-INTRUSION", "DEAUTH", "PWNAGOTCHI",
        "MULTISSID", "FLOCK", "PINEAPPLE", "AIRTAG", "FLIPPER", "META", "AXON", "TILE", "SMARTTAG",
        "GOOGLE TAG", "EARLY WARNING"
    ] as Array<String>;

    function shortName(target as Number) as String {
        return (target >= 0 && target < SHORT.size()) ? SHORT[target] : "?";
    }
    function displayName(target as Number) as String {
        return (target >= 0 && target < DISPLAY.size()) ? DISPLAY[target] : "?";
    }

    // The Recon mode line from Status flags and the selection, as the home
    // screen and the Recon page show it.
    function modeText(flags as Number, selected as Number) as String {
        if ((flags & Link.FLAG_MONITORING) != 0) { return "RECON " + shortName(selected); }
        if ((flags & Link.FLAG_EARLY_WARNING_ENABLED) != 0) {
            return (flags & Link.FLAG_EARLY_WARNING_RESTING) != 0 ? "EARLY WARN  REST" : "EARLY WARN";
        }
        return "RECON OFF";
    }

    function countText(n as Number) as String {
        return n == 1 ? "1 DETECTION" : n.format("%d") + " DETECTIONS";
    }

    const CONFIDENCE = ["LOW", "MEDIUM", "HIGH"] as Array<String>;

    function confidenceName(c as Number) as String {
        return (c >= 0 && c < CONFIDENCE.size()) ? CONFIDENCE[c] : "?";
    }
}
