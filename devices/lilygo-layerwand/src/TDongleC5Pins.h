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

// Pin map for the LILYGO T-Dongle-C5 (ESP32-C5HR8), Slice 1 Increment 0.
//
// Sources:
//   - LILYGO schematic "T-Dongle-C5 V1.1" (hardware/T-Dongle-C5 V1.1.pdf in
//     github.com/Xinyuan-LilyGO/T-Dongle-C5, commit 239a993).
//   - LILYGO include/pin_config.h in the same repository and commit.
// Where the two agree, the value is used as is. Notes below record what the
// schematic adds.

#include <stdint.h>

namespace layertime {
namespace tdongle_c5 {
namespace pins {

// ST7735 0.96 in 80 x 160 LCD on SPI. The LCD shares SCK/MOSI with the
// microSD slot; the SD card is not used in Slice 1 (decision D5).
constexpr int8_t kLcdMosi = 2;
constexpr int8_t kLcdSck = 6;
constexpr int8_t kLcdCs = 10;
constexpr int8_t kLcdDc = 3;
constexpr int8_t kLcdRst = 1;
// Backlight: GPIO0 drives the gate of Q1, an SI2301 P-channel MOSFET on the
// LCD LED supply (schematic V1.1). Low turns the backlight on.
constexpr int8_t kLcdBacklight = 0;
constexpr bool kLcdBacklightOnLevel = false;

// Panel geometry and RAM offsets. The visible 80 x 160 area starts at
// column 26, row 1 of the ST7735's RAM (LILYGO lib/lcd_st7735/st7735.h:
// "BGR, inverted, 26 / 1 offset"). Used in landscape, 160 x 80.
constexpr uint16_t kLcdNativeWidth = 80;
constexpr uint16_t kLcdNativeHeight = 160;
constexpr uint16_t kLcdColumnOffset = 26;
constexpr uint16_t kLcdRowOffset = 1;

// One APA102 RGB LED, powered from 3.3 V (schematic V1.1, U3). The schematic
// and LILYGO's pin table put its clock on GPIO 4 and data on GPIO 5, but on
// the boards tested it answers the LCD's SPI bus (kLcdSck, kLcdMosi) instead,
// and nothing on GPIO 4 or 5 reaches it (Michael, 2026-10-01; see C5Led.h).
// GPIO 4 and 5 are left untouched.

// BOOT button KEY1 on GPIO28, to ground, with a 10 k pull-up (R16) to 3.3 V
// (schematic V1.1). Pressed reads low. GPIO28 is a strapping pin: held low
// at power-up it selects download mode (ESP32-C5 datasheet, Table 3-3).
constexpr int8_t kBootButton = 28;

} // namespace pins
} // namespace tdongle_c5
} // namespace layertime
