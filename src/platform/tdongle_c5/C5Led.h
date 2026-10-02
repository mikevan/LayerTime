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

// The T-Dongle-C5's single APA102 RGB LED.
//
// On the boards tested (both LayerWands, Michael, 2026-10-01) the LED takes
// its clock and data from the LCD's SPI bus (clock GPIO 6, data GPIO 2), not
// from GPIO 4 and 5 as LILYGO's schematic V1.1 and pin table show: nothing
// sent on GPIO 4 or 5 ever reached it (our driver, LILYGO's own APA102
// driver, and a single-wire driver), it stayed dark while the LCD was never
// started, every LCD transfer changed it, and an off frame sent over SPI
// with the LCD deselected turned it off.
//
// So frames go out over SPI with the LCD's chip select held high (the LCD
// ignores them), and every LCD transfer scrambles the LED: whoever writes
// the LCD must call show() again afterwards (C5Display::transfers()).

#include "BringUpLogic.h"

namespace layertime {
namespace tdongle_c5 {

class C5Led {
public:
    // Holds the LCD deselected, starts the SPI bus on the LCD's pins if the
    // LCD has not (starting it clocks nothing), and turns the LED off.
    void begin();
    void show(const Rgb &color, uint8_t brightness);
    void off();
};

} // namespace tdongle_c5
} // namespace layertime
