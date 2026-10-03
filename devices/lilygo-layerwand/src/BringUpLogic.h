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

#include "core/model/MonitorEvent.h"

namespace layertime {
namespace tdongle_c5 {

// --- APA102 --------------------------------------------------------------
//
// One update of a single APA102 is a start frame of 32 zero bits, one LED
// frame, and a trailing frame of 32 zero bits. The LED frame is 0b111
// followed by a 5-bit global brightness, then blue, green, red.
//
// Every update must be a whole number of 32-bit words. The LED reads the
// stream in 32-bit words, so an update of any other length shifts where the
// next one is read from: the earlier 1-byte end frame (72 bits) made every
// later colour, brightness and "off" decode from the wrong bits (Michael's
// light test, 2026-10-01: off showed purple, a white brightness ramp cycled
// colours). LilyGo's own driver sends 64 bits for one LED. The trailing 32
// zero bits are the reset frame the SK9822 clone needs to show the frame it
// just received (cpldcpu, "SK9822 - a clone of the APA102?", 2016), and
// they leave the data line low.
struct Rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

constexpr size_t kApa102FrameBytes = 4 + 4 + 4;
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

// Short and long presses of the same active-low button (LayerWand, Increment
// 2B). A press must read pressed for kDebounceMs without a break to count.
// Held for kLongPressMs it reports Long once, while still held; released
// before that it reports Short once, after the release has read released for
// kDebounceMs. Releasing re-arms.
enum class ButtonGesture : uint8_t { None, Short, Long };

class ButtonGestures {
public:
    static constexpr uint32_t kDebounceMs = 30;
    static constexpr uint32_t kLongPressMs = 3000;

    ButtonGesture update(bool pressed, uint32_t nowMs);

private:
    bool _lastRaw = false;
    uint32_t _rawSinceMs = 0;
    bool _down = false;
    bool _longSent = false;
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

// --- LayerWand screen: owl indicators --------------------------------------
//
// The owl's green eye shows the watch link: red with no LayerTime watch
// connected, green with one. The green lens below it is reserved for the
// LayerWand mesh role over 802.15.4: red not connected, blue node, green
// master (Michael, 2026-10-02). The mesh is designed later, so the lens
// shows NotConnected until then.
//
// An indicator recolours only the green pixels inside its box and keeps
// their shading: each pixel's green level, measured against kOwlGreenPeak,
// scales the target colour. Green leaves the pixels as drawn. Red is pure
// red, not the owl's orange-red, so a red indicator stands out from the owl.
enum class LinkIndicator : uint8_t { NoWatch, Connected };
enum class MeshIndicator : uint8_t { NotConnected, Node, Master };
enum class IndicatorColor : uint8_t { Green, Red, Blue };

IndicatorColor linkIndicatorColor(LinkIndicator link);
IndicatorColor meshIndicatorColor(MeshIndicator mesh);

// Rows top..bottom and columns left..right, inclusive.
struct PixelBox {
    uint8_t top;
    uint8_t bottom;
    uint8_t left;
    uint8_t right;
};

constexpr uint8_t kOwlGreenPeak = 230;
constexpr Rgb kIndicatorRed{255, 0, 0};
constexpr Rgb kIndicatorBlue{32, 96, 255};

// True when the RGB565 pixel's green level is above its red and its blue.
bool isGreenPixel(uint16_t rgb565);
// The pixel in the indicator colour, shaded as it was; unchanged when it is
// not green or the colour is Green.
uint16_t recolorGreenPixel(uint16_t rgb565, IndicatorColor color);
// pixels: RGB565, little-endian (LVGL's native order), width pixels a row.
// The box must lie inside the image.
void recolorOwlIndicator(uint8_t *pixels, uint16_t width, const PixelBox &box, IndicatorColor color);

// --- LayerWand screen: events by band ---------------------------------------
//
// Which screen total an event counts toward. BLE events are BLE whatever
// their band field says. A Wi-Fi event uses its band, or its channel when
// the band is unknown: 1 to 14 is 2.4 GHz, 32 and up is 5 GHz.
enum class EventBand : uint8_t { Other, Ble, Wifi2_4GHz, Wifi5GHz };

EventBand eventBand(SourceKind kind, Band band, uint8_t channel);

// Totals since power-on. Not cleared when the watch clears the event list.
struct BandTotals {
    uint32_t ble = 0;
    uint32_t wifi2_4GHz = 0;
    uint32_t wifi5GHz = 0;
    uint32_t other = 0;

    void add(EventBand band);
};

// --- LayerWand screen: other LayerWands nearby ------------------------------
//
// Other LayerWands heard advertising the LayerTime service, by Bluetooth
// address, and how many were heard in the last kWindowMs. A LayerWand
// advertises while no watch is connected to it. When the mesh exists, its
// peers replace this count. Full, a new address takes the slot heard
// longest ago. Safe across millis() wraparound.
class PeerSightings {
public:
    static constexpr uint8_t kCapacity = 32;
    static constexpr uint32_t kWindowMs = 5u * 60u * 1000u;

    void saw(const uint8_t address[6], uint32_t nowMs);
    uint8_t count(uint32_t nowMs) const;

private:
    struct Entry {
        uint8_t address[6];
        uint32_t lastMs;
        bool used;
    };
    Entry _entries[kCapacity] = {};
};

// --- LayerWand channel plan (commit 2, 2026-10-02) -------------------------
//
// Michael's scan order: every 5 GHz channel, lowest to highest, then every
// 2.4 GHz channel, lowest to highest (BLE follows the pass in the
// scheduler). The candidates are every channel ESP-IDF names for the
// ESP32-C5 (wifi_5g_channel_bit_t, and 2.4 GHz 1 to 14); the plan keeps the
// ones the radio accepts under the regulatory rules it is running with.
constexpr uint8_t kWifi5GHzCandidates[] = {36,  40,  44,  48,  52,  56,  60,  64,  100, 104,
                                           108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
                                           149, 153, 157, 161, 165, 169, 173, 177};
constexpr uint8_t kWifi2_4GHzCandidates[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

// True when the radio accepts the channel.
using ChannelAccepted = bool (*)(uint8_t channel, void *context);

// Fills out with the accepted candidates, 5 GHz first, each band lowest to
// highest, stopping at capacity. Returns how many it wrote.
uint8_t buildChannelPlan(ChannelAccepted accepted, void *context, uint8_t *out, uint8_t capacity);

} // namespace tdongle_c5
} // namespace layertime
