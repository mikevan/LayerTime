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

// Hardware-free logic behind the LayerWand's SD-card log. Nothing here
// touches a pin, the card, or a radio, so it builds and is tested with plain
// g++ (test/test_tdongle_c5_bringup/).
//
// The log (Michael, approved 2026-10-01; changed 2026-10-03 to log from
// boot):
//   - The card shares its bus with the status LED, so every card access
//     flashes it. The card is touched only when it must be (option c,
//     Michael, 2026-10-03): once right after boot to find it, then only
//     when a write is due, which is when half the record ring is waiting or
//     when the watch connects or disconnects. A card inserted after boot is
//     found at the next write. Up to half the ring is lost if power is cut.
//   - With a card, every record goes to it for the whole run, watch
//     connected or not. A card that fails (removed, write error) is looked
//     for again at the next write, and gets a new file.
//   - Without a card, events live only in the LayerWand's memory (the newest
//     40). Link Status says so (noSdLog) and the watch warns the user.
//   - Files are layerwand_0001.log, layerwand_0002.log, and so on: the next
//     serial after the highest one already on the card. The LayerWand has no
//     clock, so date names wait for a watch time-set command.
//   - One CSV line per record: boots, events, link changes, and Recon mode
//     changes. Every text field is quoted. SSIDs and device names come off
//     the air and are untrusted, so a quote is doubled and a control byte or
//     backslash is written as \xNN or \\ (nothing is dropped). Core's
//     DetectionCsv carries a known quoting defect; this log does not share
//     its formatter.

#include <stddef.h>
#include <stdint.h>

#include "core/model/MonitorEvent.h"

namespace layertime {
namespace tdongle_c5 {

// --- When to write ---------------------------------------------------------

class SdWritePolicy {
public:
    explicit SdWritePolicy(uint16_t ringCapacity) : _threshold(ringCapacity / 2) {}

    // True when a write is due: the watch connected or disconnected (and
    // something waits), or half the ring is waiting.
    bool due(uint16_t queued, bool linkChanged) const
    {
        if (queued == 0) return false;
        if (linkChanged) return true;
        return _threshold > 0 && queued >= _threshold;
    }
    uint16_t threshold() const { return _threshold; }

private:
    uint16_t _threshold;
};

// --- File names ----------------------------------------------------------

constexpr uint16_t kMaxLogSerial = 9999;
constexpr size_t kLogFileNameSize = sizeof("layerwand_0000.log");

// The serial in "layerwand_NNNN.log" (any letter case, an optional leading
// '/'), or 0 when the name is anything else.
uint16_t logFileSerial(const char *name);

// Writes "layerwand_NNNN.log". False if the serial is out of range (1 to
// kMaxLogSerial) or the buffer is too small.
bool logFileName(uint16_t serial, char *out, size_t size);

// --- Records -------------------------------------------------------------

enum class SdRecordKind : uint8_t { Boot = 0, Event = 1, Link = 2, Mode = 3 };

struct SdRecord {
    static constexpr uint8_t kTextSize = 40;

    SdRecordKind kind = SdRecordKind::Event;
    uint32_t uptimeMs = 0;
    MonitorEvent event;              // Event only
    char text[kTextSize] = {0};      // Boot: reset reason; Link: connected or
                                     // disconnected; Mode: the mode name
    char peer[MonitorEvent::kSourceIdSize] = {0}; // Link: the watch's address
};

// A fixed ring of records in storage the platform provides (PSRAM on the
// LayerWand). Not locked: the platform guards push and pop.
class SdRecordRing {
public:
    void attach(SdRecord *storage, uint16_t capacity);
    bool push(const SdRecord &record); // false, and dropped() + 1, when full
    bool pop(SdRecord &out);
    void clear() { _head = _count = 0; }
    uint16_t size() const { return _count; }
    uint16_t capacity() const { return _capacity; }
    uint32_t dropped() const { return _dropped; }

private:
    SdRecord *_storage = nullptr;
    uint16_t _capacity = 0;
    uint16_t _head = 0;
    uint16_t _count = 0;
    uint32_t _dropped = 0;
};

// --- CSV -----------------------------------------------------------------

// The column line every file starts with, ending in "\r\n".
extern const char kSdLogHeader[];

// Writes text as one quoted CSV field: '"' doubled, '\\' as "\\\\", and any
// byte below 0x20 or 0x7F as "\\xNN". Returns the length written (no
// terminator counted), or 0 if it does not fit with its terminator.
size_t csvQuoted(const char *text, char *out, size_t size);

// One record as a CSV line ending in "\r\n". detectorName is the core's name
// for record.event.detector (the platform passes recon::detectorName(), so
// this file needs nothing beyond the model). Returns the length, or 0 if the
// line does not fit.
size_t formatSdRecord(const SdRecord &record, uint32_t bootCount, const char *detectorName, char *out,
                      size_t size);

} // namespace tdongle_c5
} // namespace layertime
