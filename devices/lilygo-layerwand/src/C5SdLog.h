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

// The LayerWand's SD-card log (Michael, approved 2026-10-01). The rules
// (when a file opens and closes, its name, its CSV) are in SdLogLogic.h and
// host-tested; this class is the card and the bus.
//
// Two tasks touch it:
//   - append() is core's EventLog port. Core calls it once per new event,
//     possibly from a radio task (EventLog.h), so it only copies the event
//     into a ring in PSRAM, under a spinlock.
//   - Everything else, the card included, runs on the loop task.
//
// Internal RAM: the ring is in PSRAM. Once found, the card stays mounted
// for the whole run, so the FAT driver's RAM is a standing cost. It is measured at each mount (heapBeforeMount,
// heapAfterMount).
//
// The bus: the card shares SCK and MOSI with the LCD and the status LED
// (C5Led.h), and adds MISO (GPIO 7) and its own CS (GPIO 23, held high from
// boot). Every card access scrambles the LED, so the card is touched only
// at boot and when a write is due (SdWritePolicy), service() reports when
// it used the bus, and the app sends the LED its state again.

#if defined(LAYERTIME_TARGET_TDONGLE_C5)

#include "SdLogLogic.h"

#include "core/ports/EventLog.h"

#include <FS.h>
#include <freertos/FreeRTOS.h>

namespace layertime {
namespace tdongle_c5 {

class C5SdLog : public EventLog {
public:
    // When to write is SdWritePolicy's (SdLogLogic.h): half this ring, or a
    // watch connect or disconnect. A write sends everything waiting at once,
    // so the LED flashes once per write.
    static constexpr uint16_t kRingCapacity = 1024;
    static constexpr uint32_t kSdHz = 4000000;
    // Longest CSV line: every text byte escaped is about 350 bytes.
    static constexpr size_t kLineSize = 512;

    struct Counters {
        bool ringOk = false;
        bool mounted = false;
        bool fileOpen = false;
        char fileName[kLogFileNameSize] = {0};
        uint32_t mounts = 0;
        uint32_t mountFailures = 0;
        uint32_t files = 0;
        uint32_t lines = 0;       // lines written, header and boot lines included
        uint32_t writes = 0;      // completed periodic or closing writes
        uint32_t writeErrors = 0; // failed open, write, or flush; the card is then unmounted
        uint32_t discarded = 0;   // records not kept: no card
        uint32_t unformatted = 0; // records too long to format (expected 0)
        uint32_t heapBeforeMount = 0;
        uint32_t heapAfterMount = 0;
    };

    // Holds GPIO 23 (the card's CS) high. Call first thing in setup().
    static void holdCardDeselected();

    // Allocates the ring in PSRAM. False if it could not; every record is
    // then counted as dropped.
    bool begin();

    void append(const MonitorEvent &event) override;

    // Loop task.
    void noteLink(bool connected, const char *peer, uint32_t nowMs);
    void noteMode(const char *mode, uint32_t nowMs);
    // Loop task, every pass. The first call looks for the card; after that
    // the card is touched only when a write is due. linkChanged: the watch
    // connected or disconnected on this pass. True if it used the SPI bus.
    bool service(uint32_t nowMs, uint32_t bootCount, const char *resetReason, bool linkChanged);
    // True while every record reaches a file on the card.
    bool hasCardLog() const { return _c.fileOpen; }

    const Counters &counters() const { return _c; }
    uint16_t queued();
    uint32_t dropped();

private:
    bool push(const SdRecord &record);
    bool pop(SdRecord &out);
    void discardQueued();
    bool openNewFile(uint32_t nowMs, uint32_t bootCount, const char *resetReason);
    // Writes up to maxLines queued records; false on a write error.
    bool writeQueued(uint32_t bootCount, uint16_t maxLines);
    bool writeLine(const char *line, size_t length);
    void closeAndUnmount();

    portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
    SdRecordRing _ring;
    SdWritePolicy _policy{kRingCapacity};
    bool _bootLookDone = false;
    fs::File _file;
    Counters _c;
};

} // namespace tdongle_c5
} // namespace layertime

#endif
