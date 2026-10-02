// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
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

// Phase 1 bring-up: the pure decisions behind the boot report, kept out of
// the hardware code so they are tested with plain g++
// (devices/lilygo-s3plus/test/test_s3plus_bringup).
//
// Expected parts come from LilyGo's T-Watch S3 Plus hardware document
// (LilyGoLib docs/hardware/lilygo-t-watch-s3-plus.md, "I2C Devices Address"),
// not from what any one watch happens to report.

#include <stddef.h>
#include <stdint.h>

namespace layertime {
namespace twatch_s3plus {
namespace bringup {

struct ExpectedDevice {
    uint8_t address;
    const char *part;
};

// Main I2C bus, Wire: SDA 10, SCL 11.
constexpr ExpectedDevice kMainBus[] = {
    {0x19, "BMA423 accelerometer"},
    {0x34, "AXP2101 power manager"},
    {0x51, "PCF8563 RTC"},
    {0x5A, "DRV2605 haptic driver"},
};

// Touch bus, Wire1: SDA 39, SCL 40.
constexpr ExpectedDevice kTouchBus[] = {
    {0x38, "FT6336U touch"},
};

// A set of 7-bit addresses that answered a scan.
struct Found {
    bool present[128] = {false};
    void add(uint8_t address) { if (address < 128) present[address] = true; }
    bool has(uint8_t address) const { return address < 128 && present[address]; }
};

// Every expected device answered.
template <size_t N>
bool allPresent(const ExpectedDevice (&expected)[N], const Found &found)
{
    for (const auto &e : expected) {
        if (!found.has(e.address)) return false;
    }
    return true;
}

// Addresses that answered but are not in the expected list. Returns how many
// were written to out (at most outSize).
template <size_t N>
size_t unexpected(const ExpectedDevice (&expected)[N], const Found &found, uint8_t *out, size_t outSize)
{
    size_t n = 0;
    for (uint8_t a = 1; a < 127; ++a) {
        if (!found.has(a)) continue;
        bool known = false;
        for (const auto &e : expected) known = known || e.address == a;
        if (!known && n < outSize) out[n++] = a;
    }
    return n;
}

enum class GnssModule : uint8_t {
    None,      // nothing answered the probe
    MiaM10Q,   // u-blox MIA-M10Q: the UBX work applies
    Ls550g,    // Quectel LS550G: NMEA and Quectel commands, no UBX
    Other,     // answered, but not a module LayerTime knows
};

// LilyGoLib's GPS::getModel(): "MIA-M10Q" from the u-blox MON-VER "MOD="
// extension, "LS550G" from the Quectel probe, empty when nothing answered.
GnssModule classifyGnss(const char *model);

const char *gnssName(GnssModule module);

// The PSRAM this watch must have: ESP32-S3-R8, 8 MB octal (schematic
// T_WATCH-S3 25-03-24). Anything else means the board or build is wrong.
constexpr uint32_t kExpectedPsramBytes = 8u * 1024u * 1024u;

// psramSize is what the allocator reports. The heap reports slightly less
// than the chip size, so this accepts anything above 7.5 MB and at most 8 MB.
bool psramAsExpected(bool found, uint32_t psramSize);

// ---- 0.1.2: FFat repair gate ------------------------------------------------

// The only partition the repair may touch: devices/lilygo-s3plus/partitions.csv, "ffat",
// data/fat at 0x810000, 0x7E0000 bytes. Anything else disables the control.
constexpr const char *kFfatLabel = "ffat";
constexpr uint32_t kFfatAddress = 0x810000u;
constexpr uint32_t kFfatSize = 0x7E0000u;

// label may be nullptr. fatPartitionCount is how many data/fat partitions
// the table has; it must be exactly one.
bool ffatPartitionIsExpected(const char *label, uint32_t address, uint32_t size,
                             unsigned fatPartitionCount);

// A continuous press-and-hold that fires once. update() is called with the
// current pressed state and time; releasing before holdMs cancels; after it
// fires it never fires again (one format per boot, at most).
class HoldGate {
public:
    enum class Event : uint8_t { None, Started, Cancelled, Fired };

    explicit HoldGate(uint32_t holdMs) : _holdMs(holdMs) {}

    Event update(bool pressed, uint32_t nowMs);
    bool holding() const { return _holding; }
    bool fired() const { return _fired; }
    // 0..100 while holding, else 0.
    uint8_t percent(uint32_t nowMs) const;

private:
    uint32_t _holdMs;
    uint32_t _startMs = 0;
    bool _holding = false;
    bool _fired = false;
};

// ---- 0.1.3: storage reboot test ---------------------------------------------

// The outcome of the check that runs on the boot after a test file was
// written. Stored in NVS as its numeric value, so the values never change
// meaning; new outcomes go at the end.
enum class RebootTestResult : uint8_t {
    None = 0,         // no test has completed on this watch
    Pass = 1,         // the file was there, matched its token, and was removed
    NotMounted = 2,   // storage was not mounted at boot; nothing could be checked
    FileMissing = 3,  // the file did not survive the reboot
    Mismatch = 4,     // the file survived but its contents changed
    NotRemoved = 5,   // the file matched but could not be removed afterwards
    WriteFailed = 6,  // the test file could not be written and read back
};

// Folds what the boot check observed into one result. A content check only
// counts when the file existed; a pass also needs the removal to succeed.
RebootTestResult classifyRebootTest(bool mounted, bool existed, bool matched, bool removed);

// A full sentence for the report.
const char *rebootTestText(RebootTestResult result);

// Reads a stored value back; anything unknown is reported as None.
RebootTestResult rebootTestFromStored(uint8_t stored);

} // namespace bringup
} // namespace twatch_s3plus
} // namespace layertime
