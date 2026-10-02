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

// The T-Dongle-C5 status LCD: ST7735, 80 x 160, used in landscape (160 x 80).
// Driven by LVGL's own ST7735 driver; this class supplies the SPI transport,
// the backlight, the LayerTime owl on the left, and five lines of status
// text on the right. The LCD is a status display, not a menu.

#include <stdint.h>

namespace layertime {
namespace tdongle_c5 {

class C5Display {
public:
    static constexpr uint8_t kLineCount = 5;

    // Initialises SPI, the panel, and LVGL. Returns false if LVGL could not
    // create the display.
    bool begin();

    // Sets one line of status text (0 .. kLineCount - 1). The column is 96 px
    // wide: about 14 characters of Montserrat 12. Longer text is clipped.
    void setLine(uint8_t line, const char *text);

    // Runs LVGL. Call from loop().
    void service();

    void setBacklight(bool on);

    // How many SPI transfers the LCD has made. The status LED shares the bus
    // and is scrambled by every one (C5Led.h), so a change means the LED must
    // be sent its state again.
    uint32_t transfers() const;
};

} // namespace tdongle_c5
} // namespace layertime
