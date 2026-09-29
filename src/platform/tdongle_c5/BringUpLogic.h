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

// Hardware-free logic behind the T-Dongle-C5 bring-up firmware (Slice 1,
// Increment 0). Nothing here touches a pin or a radio, so it builds and is
// tested with plain g++ (test/test_tdongle_c5_bringup/).

#include <stddef.h>
#include <stdint.h>

namespace layertime {
namespace tdongle_c5 {

// --- APA102 --------------------------------------------------------------
//
// One update of a single APA102 is a start frame of 32 zero bits, one LED
// frame, and an end frame. The LED frame is 0b111 followed by a 5-bit global
// brightness, then blue, green, red. The end frame is one zero byte: a
// single LED needs no extra clock edges to latch, and the byte leaves the
// data line low.
struct Rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

constexpr size_t kApa102FrameBytes = 4 + 4 + 1;
constexpr uint8_t kApa102MaxBrightness = 31;

// Fills out[0..kApa102FrameBytes). Brightness above 31 is clamped to 31.
void encodeApa102(const Rgb &color, uint8_t brightness, uint8_t out[kApa102FrameBytes]);

// The bring-up LED cycle: red, green, blue, one step per call index.
Rgb bringUpCycleColor(uint32_t step);

// --- Button --------------------------------------------------------------
//
// Debounces an active-low push button. A press is counted once, when the
// input has read pressed for kDebounceMs without a break. Releasing re-arms.
class ButtonDebouncer {
public:
    static constexpr uint32_t kDebounceMs = 30;

    // pressed: the raw reading, already converted to "true = pressed".
    // Returns true exactly once per debounced press.
    bool update(bool pressed, uint32_t nowMs);

    bool isPressed() const { return _stablePressed; }

private:
    bool _lastRaw = false;
    bool _stablePressed = false;
    uint32_t _rawSinceMs = 0;
};

// --- BLE test name -------------------------------------------------------
//
// "LT-C5-" followed by the last two bytes of the Bluetooth MAC in upper-case
// hex, for example "LT-C5-3FA2". out must hold kAdvertisedNameSize bytes.
constexpr size_t kAdvertisedNameSize = 11; // 10 characters + terminator
void formatAdvertisedName(const uint8_t mac[6], char out[kAdvertisedNameSize]);

// --- Units ---------------------------------------------------------------
//
// Whole mebibytes, rounded down, for the serial and LCD report.
uint32_t wholeMiB(uint32_t bytes);

} // namespace tdongle_c5
} // namespace layertime
