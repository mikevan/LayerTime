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

// Platform-independent LayerTime core. Nothing under src/core may include
// Arduino, LVGL, LilyGoLib, or any other platform header. Canonical
// definition: contracts/models.md.

#include <stdint.h>

namespace layertime {

// A point in time as a device knows it.
//
// uptimeMs is always present and is only meaningful on the device that
// produced it. It is what ages ("seen 40 s ago") are computed from on that
// device. It must never be compared across devices.
//
// unixSeconds is only present when the producing device had a trustworthy
// wall clock. It is what crosses device boundaries. When wallClockValid is
// false, unixSeconds is 0 and must not be displayed.
struct Timestamp {
    uint32_t uptimeMs = 0;
    bool wallClockValid = false;
    uint32_t unixSeconds = 0;
};

} // namespace layertime
