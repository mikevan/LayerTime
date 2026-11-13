// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
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

#pragma once

// The declination instruction shown on the MAPPING screen: which offset to
// apply for the chosen map north, and the words that tell the wearer what to
// do with it. Moved unchanged from MappingScreen in Phase 0 Step 3b.
//
// The text is returned as printf-style formats plus the value to format,
// not as finished strings. The screen still formats them through LVGL's own
// lv_label_set_text_fmt(), exactly as before, so the device's rounding and
// output are byte-for-byte what they were before the move.

#include <stdint.h>

namespace layertime {
namespace declination {

enum class MapNorth : uint8_t {
    Grid = 0,  // UTM/MGRS map: apply the G-M angle
    True = 1,  // true-north map: apply the plain declination
};

// Grid north: declination minus grid convergence (the military G-M angle).
// True north: the declination itself.
double mapOffsetDegrees(double declinationDeg, double convergenceDeg, MapNorth north);

// Shown when there is no location to compute from.
extern const char *const kNoLocationValue;
extern const char *const kNoLocationAdvice;

struct Instruction {
    // Always zero or positive. The sign is carried by which format was chosen.
    double magnitude = 0.0;
    // One %.1f, for the big value line, e.g. "%.1f DEG EAST".
    const char *valueFormat = nullptr;
    // One %.1f then one %s, for the advice sentence.
    const char *adviceFormat = nullptr;
    // The %s argument for adviceFormat: "grid" or "true".
    const char *northName = nullptr;
};

// Zero or positive offsets read EAST and say ADD; negative read WEST and
// say SUBTRACT.
Instruction instructionFor(double offsetDeg, MapNorth north);

} // namespace declination
} // namespace layertime
