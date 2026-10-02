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

// Built by the tdongle_c5 environment and by the host tests. The T-Watch
// Ultra build, which compiles all of src/, sees an empty translation unit.
#if !defined(ARDUINO) || defined(LAYERTIME_TARGET_TDONGLE_C5)

#include "BringUpLogic.h"

namespace layertime {
namespace tdongle_c5 {

void encodeApa102(const Rgb &color, uint8_t brightness, uint8_t out[kApa102FrameBytes])
{
    if (brightness > kApa102MaxBrightness) brightness = kApa102MaxBrightness;
    out[0] = 0x00;
    out[1] = 0x00;
    out[2] = 0x00;
    out[3] = 0x00;
    out[4] = static_cast<uint8_t>(0xE0 | brightness);
    out[5] = color.blue;
    out[6] = color.green;
    out[7] = color.red;
    out[8] = 0x00;
    out[9] = 0x00;
    out[10] = 0x00;
    out[11] = 0x00;
}

Rgb bringUpCycleColor(uint32_t step)
{
    switch (step % 3) {
    case 0: return Rgb{255, 0, 0};
    case 1: return Rgb{0, 255, 0};
    default: return Rgb{0, 0, 255};
    }
}

bool ButtonDebouncer::update(bool pressed, uint32_t nowMs)
{
    if (pressed != _lastRaw) {
        _lastRaw = pressed;
        _rawSinceMs = nowMs;
        return false;
    }
    if (pressed == _stablePressed) return false;
    if (nowMs - _rawSinceMs < kDebounceMs) return false;
    _stablePressed = pressed;
    return pressed;
}

ButtonGesture ButtonGestures::update(bool pressed, uint32_t nowMs)
{
    if (pressed != _lastRaw) {
        _lastRaw = pressed;
        _rawSinceMs = nowMs;
        return ButtonGesture::None;
    }
    const bool settled = nowMs - _rawSinceMs >= kDebounceMs;
    if (!_down) {
        if (pressed && settled) {
            _down = true;
            _longSent = false;
        }
        return ButtonGesture::None;
    }
    if (!pressed) {
        if (!settled) return ButtonGesture::None;
        _down = false;
        return _longSent ? ButtonGesture::None : ButtonGesture::Short;
    }
    if (!_longSent && nowMs - _rawSinceMs >= kLongPressMs) {
        _longSent = true;
        return ButtonGesture::Long;
    }
    return ButtonGesture::None;
}

void formatAdvertisedName(const uint8_t mac[6], char out[kAdvertisedNameSize])
{
    static const char kHex[] = "0123456789ABCDEF";
    const char prefix[] = "LT-C5-";
    size_t n = 0;
    for (; prefix[n] != '\0'; ++n) out[n] = prefix[n];
    out[n++] = kHex[mac[4] >> 4];
    out[n++] = kHex[mac[4] & 0x0F];
    out[n++] = kHex[mac[5] >> 4];
    out[n++] = kHex[mac[5] & 0x0F];
    out[n] = '\0';
}

uint32_t wholeMiB(uint32_t bytes)
{
    return bytes / (1024u * 1024u);
}

} // namespace tdongle_c5
} // namespace layertime

#endif
