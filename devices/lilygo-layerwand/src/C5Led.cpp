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

// This file belongs to the tdongle_c5 build environments only
// (devices/lilygo-layerwand/platformio.ini). The guard below dates from when
// every environment compiled all of src/, and is kept as a safety net.
#if defined(LAYERTIME_TARGET_TDONGLE_C5)

#include "C5Led.h"
#include "TDongleC5Pins.h"

#include <Arduino.h>
#include <SPI.h>

namespace layertime {
namespace tdongle_c5 {

namespace {

// 1 MHz, mode 0: data is valid on the rising clock edge, where the APA102
// samples it. A 96-bit frame takes about 0.1 ms.
const SPISettings kLedSpi(1000000, MSBFIRST, SPI_MODE0);

} // namespace

void C5Led::begin()
{
    digitalWrite(pins::kLcdCs, HIGH);
    pinMode(pins::kLcdCs, OUTPUT);
    SPI.begin(pins::kLcdSck, -1, pins::kLcdMosi, -1); // returns at once if already started
    off();
}

void C5Led::show(const Rgb &color, uint8_t brightness)
{
    uint8_t frame[kApa102FrameBytes];
    encodeApa102(color, brightness, frame);
    SPI.beginTransaction(kLedSpi);
    digitalWrite(pins::kLcdCs, HIGH); // the LCD must not take these bytes
    SPI.writeBytes(frame, sizeof(frame));
    SPI.endTransaction();
}

void C5Led::off()
{
    show(Rgb{0, 0, 0}, 0);
}

} // namespace tdongle_c5
} // namespace layertime

#endif
