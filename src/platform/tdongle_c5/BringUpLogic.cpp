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

#include <string.h>

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

// --- Owl indicators ------------------------------------------------------------

namespace {

uint8_t expand5(uint16_t v) { return static_cast<uint8_t>((v * 255u + 15u) / 31u); }
uint8_t expand6(uint16_t v) { return static_cast<uint8_t>((v * 255u + 31u) / 63u); }

uint16_t pack565(uint32_t r8, uint32_t g8, uint32_t b8)
{
    const uint32_t r5 = (r8 * 31u + 127u) / 255u;
    const uint32_t g6 = (g8 * 63u + 127u) / 255u;
    const uint32_t b5 = (b8 * 31u + 127u) / 255u;
    return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

} // namespace

IndicatorColor linkIndicatorColor(LinkIndicator link)
{
    return link == LinkIndicator::Connected ? IndicatorColor::Green : IndicatorColor::Red;
}

IndicatorColor meshIndicatorColor(MeshIndicator mesh)
{
    switch (mesh) {
    case MeshIndicator::Node: return IndicatorColor::Blue;
    case MeshIndicator::Master: return IndicatorColor::Green;
    default: return IndicatorColor::Red;
    }
}

bool isGreenPixel(uint16_t rgb565)
{
    const uint8_t r = expand5((rgb565 >> 11) & 0x1F);
    const uint8_t g = expand6((rgb565 >> 5) & 0x3F);
    const uint8_t b = expand5(rgb565 & 0x1F);
    return g > r && g > b;
}

uint16_t recolorGreenPixel(uint16_t rgb565, IndicatorColor color)
{
    if (color == IndicatorColor::Green || !isGreenPixel(rgb565)) return rgb565;
    uint32_t level = expand6((rgb565 >> 5) & 0x3F);
    if (level > kOwlGreenPeak) level = kOwlGreenPeak;
    const Rgb &t = color == IndicatorColor::Red ? kIndicatorRed : kIndicatorBlue;
    return pack565(t.red * level / kOwlGreenPeak, t.green * level / kOwlGreenPeak,
                   t.blue * level / kOwlGreenPeak);
}

void recolorOwlIndicator(uint8_t *pixels, uint16_t width, const PixelBox &box, IndicatorColor color)
{
    if (color == IndicatorColor::Green) return;
    for (uint32_t y = box.top; y <= box.bottom; ++y) {
        for (uint32_t x = box.left; x <= box.right; ++x) {
            uint8_t *p = pixels + (y * width + x) * 2u;
            const uint16_t was = static_cast<uint16_t>(p[0] | (p[1] << 8));
            const uint16_t now = recolorGreenPixel(was, color);
            p[0] = static_cast<uint8_t>(now & 0xFF);
            p[1] = static_cast<uint8_t>(now >> 8);
        }
    }
}

// --- Events by band ------------------------------------------------------------

EventBand eventBand(SourceKind kind, Band band, uint8_t channel)
{
    if (kind == SourceKind::Ble) return EventBand::Ble;
    if (kind != SourceKind::Wifi) return EventBand::Other;
    if (band == Band::Band2_4GHz) return EventBand::Wifi2_4GHz;
    if (band == Band::Band5GHz) return EventBand::Wifi5GHz;
    if (channel >= 1 && channel <= 14) return EventBand::Wifi2_4GHz;
    if (channel >= 32) return EventBand::Wifi5GHz;
    return EventBand::Other;
}

void BandTotals::add(EventBand band)
{
    switch (band) {
    case EventBand::Ble: ++ble; break;
    case EventBand::Wifi2_4GHz: ++wifi2_4GHz; break;
    case EventBand::Wifi5GHz: ++wifi5GHz; break;
    default: ++other; break;
    }
}

// --- Other LayerWands nearby ---------------------------------------------------

void PeerSightings::saw(const uint8_t address[6], uint32_t nowMs)
{
    Entry *slot = nullptr;
    for (Entry &e : _entries) {
        if (e.used && memcmp(e.address, address, 6) == 0) {
            e.lastMs = nowMs;
            return;
        }
    }
    uint32_t oldestAge = 0;
    for (Entry &e : _entries) {
        if (!e.used) {
            slot = &e;
            break;
        }
        const uint32_t age = nowMs - e.lastMs;
        if (slot == nullptr || age > oldestAge) {
            slot = &e;
            oldestAge = age;
        }
    }
    memcpy(slot->address, address, 6);
    slot->lastMs = nowMs;
    slot->used = true;
}

uint8_t PeerSightings::count(uint32_t nowMs) const
{
    uint8_t n = 0;
    for (const Entry &e : _entries) {
        if (e.used && nowMs - e.lastMs < kWindowMs) ++n;
    }
    return n;
}

} // namespace tdongle_c5
} // namespace layertime

#endif
