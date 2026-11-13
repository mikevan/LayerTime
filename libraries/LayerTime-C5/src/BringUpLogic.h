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

// Short and long presses of the same active-low button (Hunter, Increment
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

// --- Unit names (Michael, 2026-11-04; base 32 from the chip, 2026-11-05) ----
//
// Every unit says what it is: "Hunter-C5-BSV72", "Vixen-C5-00FX2". The middle
// is the chip (ESP32-C5 gives "C5", so an S3 build says "S3" with no change).
// The last part is unique to the module itself: the last three bytes of its
// Bluetooth address (set from the chip's factory address) written as five
// characters of Crockford base 32. That alphabet has no I, L, O, or U, so
// nothing can be misread as 1, 0, or V. No registry and no numbering step:
// a board has its name from its first boot, and a reflash keeps it.
constexpr size_t kUnitIdSize = 6;    // five characters + terminator
constexpr size_t kUnitNameSize = 20; // fits the Link's 20-byte name buffer; longest is 18 + terminator

// "C5" from "ESP32-C5"; a model without the "ESP32-" prefix is used whole;
// null or empty gives "ESP32". Truncated to fit.
void chipShortName(const char *model, char *out, size_t size);

// Five Crockford base-32 characters for mac[3], mac[4], mac[5] (24 bits,
// most significant first, so the first character is 0 to F). out must hold
// kUnitIdSize bytes. Empty string if mac or out is null.
void unitIdBase32(const uint8_t mac[6], char *out, size_t size);

// "<kind>-<chip>-<id>". False (and out empty) if it does not fit.
bool formatUnitName(const char *kind, const char *chip, const uint8_t mac[6], char *out, size_t size);

// Serial command at the bench (one line):
//   unit get        the unit's name and Bluetooth address
// Any other "unit ..." line (an old "unit set <n>" included) is Bad, so the
// unit can say the name now comes from the chip.
enum class UnitCommand : uint8_t { None = 0, Get = 1, Bad = 3 };
UnitCommand parseUnitCommand(const char *line);

// --- Sensors that start on (Michael, 2026-11-06) ---------------------------
//
// After a field test: with the trackers on, their flood hid every other
// sensor. A Hunter with no saved sensor list starts with every sensor except
// the trackers (AirTag 10, Tile 14, SmartTag 15, Google Tag 16, the ReconTarget
// values). LayerScope sends its own list on connect, as before; this is what
// a Hunter runs without one. Kept here, not in core, so the watches' defaults
// do not change.
constexpr uint32_t kTrackerSensorBits = (1u << 10) | (1u << 14) | (1u << 15) | (1u << 16);
constexpr uint32_t kHunterDefaultSensorMask = kAllSensorsMask & ~kTrackerSensorBits;

// --- Units ---------------------------------------------------------------
//
// Whole mebibytes, rounded down, for the serial and LCD report.
uint32_t wholeMiB(uint32_t bytes);

// --- Hunter screen: eye and cheek indicators ---------------------------------
//
// On the Hunter cat (Michael, 2026-11-01; the owl's eye and lens before it):
// a green eye means a LayerTime or LayerScope is connected, and a green cheek
// means the Hunter is on the mesh (802.15.4, designed later). Either one is
// red when that is not true. Node and master both count as on the mesh; the
// old blue for a node is retired. Until the mesh exists the cheek stays red.
//
// An indicator recolours only the green pixels inside its box and keeps
// their shading: each pixel's green level, measured against kOwlGreenPeak,
// scales the target colour. Green leaves the pixels as drawn. Red is pure
// red, so a red indicator stands out from the amber head.
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
void recolorIndicator(uint8_t *pixels, uint16_t width, const PixelBox &box, IndicatorColor color);

// --- Hunter screen: events by band ---------------------------------------
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

// --- Hunter screen: other Hunters nearby ------------------------------
//
// Other Hunters heard advertising the LayerTime service, by Bluetooth
// address, and how many were heard in the last kWindowMs. A Hunter
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

// --- Hunter channel plan (commit 2, 2026-10-02) -------------------------
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
