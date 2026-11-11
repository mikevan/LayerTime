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

// The Hunter's SD-card log (Michael, approved 2026-10-01). The rules
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

#include "CardTransferLogic.h"
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
    // Old layerwand_NNNN.log files renamed per mount (the rest wait for the next mount).
    static constexpr size_t kMaxLegacyRenames = 64;
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
        uint32_t repeats = 0;        // repeat sightings queued (throttled, RepeatThrottle)
        uint32_t evidenceFiles = 0;  // evidence windows written as pcap files
        uint32_t evidenceBytes = 0;
        uint32_t evidenceErrors = 0; // failed folder, open, write, or log reopen
        uint32_t legacyRenamed = 0;  // layerwand_NNNN.log files renamed to hunter_NNNN.log
        uint32_t cardClears = 0;     // Clear Card requests carried out
    };

    // Holds GPIO 23 (the card's CS) high. Call first thing in setup().
    static void holdCardDeselected();

    // Allocates the ring in PSRAM. False if it could not; every record is
    // then counted as dropped.
    bool begin();

    void append(const MonitorEvent &event) override;
    // A repeat sighting: a `repeat` line, at most once per kRepeatEveryMs per
    // event (2026-11-04). Same task and lock rules as append.
    void repeated(const MonitorEvent &event) override;

    // Loop task. Writes one evidence window (a whole pcap file, built by the
    // caller) to ev_NNNN/event_EEEEE.pcap beside the open log, and logs an
    // `evidence` line for it in the same card access. Only while a log file is
    // open: with no card it does nothing and returns false, so the window
    // waits in PSRAM. `usedBus` says the card was touched (resend the LED).
    // True when the file was written.
    bool saveEvidence(uint32_t eventId, const uint8_t *pcap, size_t length, const SdEvidenceCounts &counts,
                      uint32_t nowMs, uint32_t bootCount, bool &usedBus);

    // Loop task.
    void noteLink(bool connected, const char *peer, uint32_t nowMs);
    void noteMode(const char *mode, uint32_t nowMs);
    // Loop task, every pass. The first call looks for the card; after that
    // the card is touched only when a write is due. linkChanged: the watch
    // connected or disconnected on this pass. True if it used the SPI bus.
    bool service(uint32_t nowMs, uint32_t bootCount, const char *resetReason, bool linkChanged);
    // Pulling files over USB serial (2026-11-04, CardTransferLogic.h). Loop
    // task only. Read only: nothing on the card is written or deleted.
    //   listCard: calls `entry` for every file the Hunter serves (the logs at
    //     the top, the pcaps in each ev_NNNN folder). False with no card.
    //   beginRead / readChunk / endRead: one file at a time. While a file is
    //     open for reading the log file is closed (the card takes one open file),
    //     so records wait in the ring and window saves wait; endRead reopens the
    //     log. `usedBus` in each: the card was touched (resend the LED).
    bool listCard(void (*entry)(const char *name, uint32_t size, void *context), void *context, bool &usedBus);
    bool beginRead(const char *name, uint32_t &size, bool &usedBus);
    size_t readChunk(uint8_t *out, size_t max, bool &usedBus);
    void endRead(bool &usedBus);
    // Busy with the card: a file open for a pull, or a clear running.
    bool reading() const { return _reading || _clearing; }
    // Clear Card (Michael, 2026-11-06, Link CARD_CLEAR): erases the card
    // COMPLETELY, every file and folder, so a card full of tracker logs can be
    // cleaned out in the field. Loop task only. beginClear closes the log and
    // starts (false while a log is not open, a file is being read, or a clear
    // runs); stepClear is called once per loop pass and erases a few files
    // each time, so scanning and the Link keep running. At the end it opens a
    // fresh hunter_0001.log; records waiting in the ring go to it. Done: the
    // card is empty but for the fresh log. Failed: something would not erase
    // (clearLeftover lists up to 16 of them), or the card went.
    enum class ClearStep : uint8_t { Idle = 0, Running = 1, Done = 2, Failed = 3 };
    bool beginClear(bool &usedBus);
    ClearStep stepClear(uint32_t nowMs, uint32_t bootCount, const char *resetReason, bool &usedBus);
    bool clearing() const { return _clearing; }
    const char *clearLeftover(uint8_t i) const; // what would not erase, or nullptr
    uint32_t clearErased() const { return _clearErased; }

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
    RepeatThrottle _repeats; // under _lock
    bool _reading = false;
    bool _clearing = false;  // Clear Card running (stepClear); the log is closed
    uint32_t _clearErased = 0;
    bool _clearClean = true;
    static constexpr uint8_t kClearSkipMax = 16;
    static constexpr size_t kClearPathSize = 96;
    static constexpr uint8_t kClearMaxDepth = 6;
    static constexpr size_t kClearBatchMax = 6;
    struct ClearBuffers {
        char files[kClearBatchMax][kClearPathSize];
        char skipped[kClearSkipMax][kClearPathSize];
    };
    // Allocate in PSRAM on the first clear. Keep failed-path results until the
    // next clear, without reserving internal radio RAM for an optional action.
    ClearBuffers *_clearBuffers = nullptr;
    int clearIn(const char *dir, uint8_t depth);
    uint8_t _clearSkipCount = 0;
    bool clearSkipped(const char *path) const;
    void skipInClear(const char *path);   // a card file is open for a USB pull; the log is closed
    fs::File _readFile;
    bool reopenLog();
    bool _bootLookDone = false;
    fs::File _file;
    Counters _c;
};

} // namespace tdongle_c5
} // namespace layertime

#endif
