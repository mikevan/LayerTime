// LayerTime - passive early-warning firmware for the LILYGO T-Dongle-C5.
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

// The T-Dongle-C5's single APA102 RGB LED, clocked out by GPIO.

#include "BringUpLogic.h"

namespace layertime {
namespace tdongle_c5 {

class C5Led {
public:
    void begin();
    void show(const Rgb &color, uint8_t brightness);
    void off();
};

} // namespace tdongle_c5
} // namespace layertime
