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

// This file belongs to the tdongle_c5 build environment only. Every build
// environment compiles all of src/, so the T-Watch Ultra build sees this file
// as an empty translation unit.
#if defined(LAYERTIME_TARGET_TDONGLE_C5)

#include "C5Led.h"
#include "TDongleC5Pins.h"

#include <Arduino.h>

namespace layertime {
namespace tdongle_c5 {

namespace {

// MSB first; data is sampled on the rising clock edge.
void clockOut(uint8_t byte)
{
    for (int bit = 7; bit >= 0; --bit) {
        digitalWrite(pins::kLedData, (byte >> bit) & 1 ? HIGH : LOW);
        digitalWrite(pins::kLedClock, HIGH);
        digitalWrite(pins::kLedClock, LOW);
    }
}

} // namespace

void C5Led::begin()
{
    pinMode(pins::kLedClock, OUTPUT);
    pinMode(pins::kLedData, OUTPUT);
    digitalWrite(pins::kLedClock, LOW);
    digitalWrite(pins::kLedData, LOW);
    off();
}

void C5Led::show(const Rgb &color, uint8_t brightness)
{
    uint8_t frame[kApa102FrameBytes];
    encodeApa102(color, brightness, frame);
    for (uint8_t b : frame) clockOut(b);
    digitalWrite(pins::kLedData, LOW);
}

void C5Led::off()
{
    show(Rgb{0, 0, 0}, 0);
}

} // namespace tdongle_c5
} // namespace layertime

#endif
