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

// Compiled only for the T-Dongle-C5 builds. The guard was needed when every
// environment compiled all of src/, and is kept as a safety net.
#if defined(LAYERTIME_TARGET_TDONGLE_C5)

#include "C5SdLog.h"
#include "C5Board.h"
#include "CardTransferLogic.h"
#include "TDongleC5Pins.h"

#include "core/logic/ReconSelection.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <new>
#include <string.h>

namespace layertime {
namespace tdongle_c5 {

namespace {

// The bus with MISO, as C5Led and C5Display start it. SPI.begin() returns at
// once if the bus is running; it matters if SD.end() stopped it, and before
// SD.begin(), which would otherwise start the bus on the default pins.
void startBus()
{
    SPI.begin(pins::kLcdSck, pins::kSdMiso, pins::kLcdMosi, -1);
    digitalWrite(pins::kLcdCs, HIGH); // the LCD must not take card traffic
}

void copyText(char *out, size_t size, const char *text)
{
    if (size == 0) return;
    strncpy(out, text == nullptr ? "" : text, size - 1);
    out[size - 1] = '\0';
}

} // namespace

void C5SdLog::holdCardDeselected()
{
    holdC5CardDeselected();
}

bool C5SdLog::begin()
{
    void *storage = heap_caps_malloc(sizeof(SdRecord) * kRingCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    SdRecord *records = nullptr;
    if (storage != nullptr) {
        records = static_cast<SdRecord *>(storage);
        for (uint16_t i = 0; i < kRingCapacity; ++i) new (&records[i]) SdRecord();
    }
    _ring.attach(records, kRingCapacity);
    _c.ringOk = records != nullptr;
    return _c.ringOk;
}

bool C5SdLog::push(const SdRecord &record)
{
    portENTER_CRITICAL(&_lock);
    const bool ok = _ring.push(record);
    portEXIT_CRITICAL(&_lock);
    return ok;
}

bool C5SdLog::pop(SdRecord &out)
{
    portENTER_CRITICAL(&_lock);
    const bool ok = _ring.pop(out);
    portEXIT_CRITICAL(&_lock);
    return ok;
}

uint16_t C5SdLog::queued()
{
    portENTER_CRITICAL(&_lock);
    const uint16_t n = _ring.size();
    portEXIT_CRITICAL(&_lock);
    return n;
}

uint32_t C5SdLog::dropped()
{
    portENTER_CRITICAL(&_lock);
    const uint32_t n = _ring.dropped();
    portEXIT_CRITICAL(&_lock);
    return n;
}

void C5SdLog::discardQueued()
{
    portENTER_CRITICAL(&_lock);
    _c.discarded += _ring.size();
    _ring.clear();
    portEXIT_CRITICAL(&_lock);
}

void C5SdLog::append(const MonitorEvent &event)
{
    SdRecord r;
    r.kind = SdRecordKind::Event;
    r.uptimeMs = millis();
    r.event = event;
    push(r);
}

void C5SdLog::repeated(const MonitorEvent &event)
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&_lock);
    const bool due = _repeats.due(event.eventId, now);
    portEXIT_CRITICAL(&_lock);
    if (!due) return;
    SdRecord r;
    r.kind = SdRecordKind::Repeat;
    r.uptimeMs = now;
    r.event = event;
    if (push(r)) ++_c.repeats;
}

bool C5SdLog::saveEvidence(uint32_t eventId, const uint8_t *pcap, size_t length, const SdEvidenceCounts &counts,
                           uint32_t nowMs, uint32_t bootCount, bool &usedBus)
{
    usedBus = false;
    if (!_c.fileOpen || _reading || _clearing || pcap == nullptr || length == 0) return false;
    const uint16_t serial = logFileSerial(_c.fileName);
    char folder[kEvidenceFolderSize + 1] = "/";
    char path[kEvidencePathSize + 1] = "/";
    if (!evidenceFolderName(serial, folder + 1, sizeof(folder) - 1) ||
        !evidencePath(serial, eventId, path + 1, sizeof(path) - 1)) {
        ++_c.evidenceErrors;
        return false;
    }
    usedBus = true;
    startBus();
    // One open file at a time (SD.begin with max_files 1): close the log,
    // write the window, then reopen the log to append.
    _file.flush();
    _file.close();
    bool ok = SD.exists(folder) || SD.mkdir(folder);
    if (ok) {
        fs::File f = SD.open(path, FILE_WRITE); // replaces an earlier save of this window
        ok = f && f.write(pcap, length) == length;
        if (f) f.close();
    }
    if (!reopenLog()) {
        // The log cannot be reopened: the card is treated as failed; the next
        // write looks for it again and opens a new log file.
        ++_c.evidenceErrors;
        return false;
    }
    if (!ok) {
        ++_c.evidenceErrors;
        return false;
    }
    ++_c.evidenceFiles;
    _c.evidenceBytes += static_cast<uint32_t>(length);

    SdRecord r;
    r.kind = SdRecordKind::Evidence;
    r.uptimeMs = nowMs;
    r.event.eventId = eventId;
    r.evidence = counts;
    copyText(r.text, sizeof(r.text), path + 1);
    char line[kLineSize];
    const size_t n = formatSdRecord(r, bootCount, "", line, sizeof(line));
    if (n == 0) ++_c.unformatted;
    else if (!writeLine(line, n)) {
        closeAndUnmount();
        return true; // the pcap is on the card; only its log line was lost
    }
    _file.flush();
    return true;
}

bool C5SdLog::reopenLog()
{
    char logPath[kLogFileNameSize + 1] = "/";
    copyText(logPath + 1, sizeof(logPath) - 1, _c.fileName);
    _file = SD.open(logPath, FILE_APPEND);
    if (_file) return true;
    _c.fileOpen = false;
    closeAndUnmount();
    return false;
}

bool C5SdLog::listCard(void (*entry)(const char *name, uint32_t size, void *context), void *context, bool &usedBus)
{
    usedBus = false;
    if (!_c.fileOpen || _reading || _clearing || entry == nullptr) return false;
    usedBus = true;
    startBus();
    _file.flush();
    _file.close();
    char folders[16][kEvidenceFolderSize] = {{0}};
    uint8_t folderCount = 0;
    fs::File root = SD.open("/");
    if (root) {
        for (fs::File f = root.openNextFile(); f; f = root.openNextFile()) {
            const char *n = f.name();
            if (n != nullptr && n[0] == '/') ++n;
            if (!f.isDirectory() && cardNameServed(n)) entry(n, static_cast<uint32_t>(f.size()), context);
            else if (f.isDirectory() && n != nullptr && strlen(n) == 7 && strncmp(n, "ev_", 3) == 0 &&
                     folderCount < 16) {
                copyText(folders[folderCount++], kEvidenceFolderSize, n);
            }
            f.close();
        }
        root.close();
    }
    // A run with more than 16 evidence folders lists the first 16 the card returns;
    // the rest are reached by a later list after older ones are copied off.
    for (uint8_t i = 0; i < folderCount; ++i) {
        char dirPath[kEvidenceFolderSize + 1] = "/";
        copyText(dirPath + 1, sizeof(dirPath) - 1, folders[i]);
        fs::File dir = SD.open(dirPath);
        if (!dir) continue;
        for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
            const char *n = f.name();
            if (n != nullptr) {
                const char *slash = strrchr(n, '/');
                if (slash != nullptr) n = slash + 1;
                char full[kCardNameSize];
                snprintf(full, sizeof(full), "%s/%s", folders[i], n);
                if (!f.isDirectory() && cardNameServed(full)) entry(full, static_cast<uint32_t>(f.size()), context);
            }
            f.close();
        }
        dir.close();
    }
    return reopenLog();
}

namespace {

// Clear Card works in batches: removing a file while its folder is open for a
// walk is not safe on this FAT driver, so each pass collects names, closes the
// folder, then removes them.

const char *leaf(const char *n)
{
    if (n == nullptr) return n;
    const char *slash = strrchr(n, '/');
    return slash != nullptr ? slash + 1 : n;
}

} // namespace

bool C5SdLog::beginClear(bool &usedBus)
{
    usedBus = false;
    if (!_c.fileOpen || _reading || _clearing) return false;
    if (_clearBuffers == nullptr) {
        void *storage = heap_caps_malloc(sizeof(ClearBuffers), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        // Refuse before touching the card if external memory is unavailable.
        if (storage == nullptr) return false;
        _clearBuffers = new (storage) ClearBuffers{};
    }
    usedBus = true;
    startBus();
    _file.flush();
    _file.close();
    _clearing = true;
    _clearErased = 0;
    _clearClean = true;
    _clearSkipCount = 0;
    return true;
}

bool C5SdLog::clearSkipped(const char *path) const
{
    for (uint8_t i = 0; i < _clearSkipCount; ++i)
        if (strcmp(_clearBuffers->skipped[i], path) == 0) return true;
    return false;
}

void C5SdLog::skipInClear(const char *path)
{
    _clearClean = false;
    if (_clearSkipCount < kClearSkipMax)
        copyText(_clearBuffers->skipped[_clearSkipCount++], sizeof(_clearBuffers->skipped[0]), path);
}

// One batch inside `dir` (a path starting with '/'): erase up to kClearBatchMax
// files, or go into the first folder and work there; an emptied folder is
// removed. Returns 1 when it did something, 0 when `dir` holds nothing it can
// still erase. Paths that would not erase go on the skip list, so a stuck
// file never loops the clear. Depth is capped so a deep tree cannot run the
// stack out.
int C5SdLog::clearIn(const char *dir, uint8_t depth)
{
    // The batch lives in PSRAM: a level that finds files erases them and
    // returns before going deeper, so one buffer serves every level.
    char (&files)[kClearBatchMax][kClearPathSize] = _clearBuffers->files;
    size_t fileCount = 0;
    char sub[kClearPathSize] = {0};
    fs::File d = SD.open(dir);
    if (!d || !d.isDirectory()) { if (d) d.close(); return 0; }
    for (fs::File f = d.openNextFile(); f; f = d.openNextFile()) {
        char path[kClearPathSize];
        const char *n = leaf(f.name());
        const int len = snprintf(path, sizeof(path), "%s%s%s", dir, (dir[0] == '/' && dir[1] == '\0') ? "" : "/", n != nullptr ? n : "");
        const bool fits = len > 0 && static_cast<size_t>(len) < sizeof(path);
        if (!fits) { _clearClean = false; f.close(); continue; } // a name too long to address: left, and reported
        if (!clearSkipped(path)) {
            if (f.isDirectory()) { if (sub[0] == '\0') copyText(sub, sizeof(sub), path); }
            else if (fileCount < kClearBatchMax) copyText(files[fileCount++], sizeof(files[0]), path);
        }
        f.close();
    }
    d.close();
    if (fileCount > 0) {
        for (size_t i = 0; i < fileCount; ++i) {
            if (SD.remove(files[i])) ++_clearErased;
            else skipInClear(files[i]);
        }
        return 1;
    }
    if (sub[0] != '\0') {
        if (depth < kClearMaxDepth && clearIn(sub, static_cast<uint8_t>(depth + 1)) == 1) return 1;
        // The folder is empty (or as empty as it will get): remove it.
        if (!SD.rmdir(sub)) skipInClear(sub);
        return 1;
    }
    return 0;
}

C5SdLog::ClearStep C5SdLog::stepClear(uint32_t nowMs, uint32_t bootCount, const char *resetReason, bool &usedBus)
{
    usedBus = false;
    if (!_clearing) return ClearStep::Idle;
    usedBus = true;
    startBus();
    // One small batch per loop pass, so scanning and the Link keep running.
    if (clearIn("/", 0) == 1) return ClearStep::Running;
    // The card is empty, apart from anything on the skip list. Start a fresh
    // log, numbered from 1 again; records waiting in the ring go to it.
    _clearing = false;
    ++_c.cardClears;
    _c.fileOpen = false;
    closeAndUnmount();
    if (!openNewFile(nowMs, bootCount, resetReason)) return ClearStep::Failed;
    return _clearClean ? ClearStep::Done : ClearStep::Failed;
}

const char *C5SdLog::clearLeftover(uint8_t i) const
{
    return _clearBuffers != nullptr && i < _clearSkipCount ? _clearBuffers->skipped[i] : nullptr;
}

bool C5SdLog::beginRead(const char *name, uint32_t &size, bool &usedBus)
{
    usedBus = false;
    size = 0;
    if (!_c.fileOpen || _reading || _clearing || !cardNameServed(name)) return false;
    usedBus = true;
    startBus();
    _file.flush();
    _file.close();
    char path[kCardNameSize + 1] = "/";
    copyText(path + 1, sizeof(path) - 1, name);
    _readFile = SD.open(path, FILE_READ);
    if (!_readFile || _readFile.isDirectory()) {
        if (_readFile) _readFile.close();
        reopenLog();
        return false;
    }
    size = static_cast<uint32_t>(_readFile.size());
    _reading = true;
    return true;
}

size_t C5SdLog::readChunk(uint8_t *out, size_t max, bool &usedBus)
{
    usedBus = false;
    if (!_reading || out == nullptr || max == 0) return 0;
    usedBus = true;
    startBus();
    const int n = _readFile.read(out, max);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

void C5SdLog::endRead(bool &usedBus)
{
    usedBus = false;
    if (!_reading) return;
    usedBus = true;
    startBus();
    _readFile.close();
    _reading = false;
    reopenLog();
}

void C5SdLog::noteLink(bool connected, const char *peer, uint32_t nowMs)
{
    SdRecord r;
    r.kind = SdRecordKind::Link;
    r.uptimeMs = nowMs;
    copyText(r.text, sizeof(r.text), connected ? "connected" : "disconnected");
    copyText(r.peer, sizeof(r.peer), peer);
    push(r);
}

void C5SdLog::noteMode(const char *mode, uint32_t nowMs)
{
    SdRecord r;
    r.kind = SdRecordKind::Mode;
    r.uptimeMs = nowMs;
    copyText(r.text, sizeof(r.text), mode);
    push(r);
}

bool C5SdLog::service(uint32_t nowMs, uint32_t bootCount, const char *resetReason, bool linkChanged)
{
    // A USB pull holds the card's one open file; records keep queuing in the ring.
    if (_reading || _clearing) return false;
    bool due;
    if (!_bootLookDone) {
        _bootLookDone = true; // once, right after boot: find the card
        due = true;
    } else {
        due = _policy.due(queued(), linkChanged);
    }
    if (!due) return false;

    if (!_c.fileOpen && !openNewFile(nowMs, bootCount, resetReason)) {
        discardQueued(); // no card: nothing to keep them for
        return true;
    }
    // Everything waiting goes out in one write, so the LED flashes once.
    if (!writeQueued(bootCount, kRingCapacity)) {
        // The card failed (removed, write error). The next write looks
        // for a card again, and that card gets a new file.
        closeAndUnmount();
        return true;
    }
    _file.flush();
    ++_c.writes;
    return true;
}

bool C5SdLog::openNewFile(uint32_t nowMs, uint32_t bootCount, const char *resetReason)
{
    startBus();
    _c.heapBeforeMount = ESP.getFreeHeap();
    // One open file at a time keeps the FAT driver's RAM to the minimum.
    if (!SD.begin(pins::kSdCs, SPI, kSdHz, "/sd", 1)) {
        ++_c.mountFailures;
        SD.end();
        startBus();
        holdCardDeselected();
        return false;
    }
    _c.heapAfterMount = ESP.getFreeHeap();
    _c.mounted = true;
    ++_c.mounts;

    // Old logs (layerwand_NNNN.log, before 2026-11-06) are renamed to
    // hunter_NNNN.log, keeping the number, so the card holds one spelling and
    // their ev_NNNN folders still match. Renames wait until the folder walk is
    // done; a rename that fails, or whose new name is taken, leaves the file as
    // it is, and its number still counts toward the next serial.
    uint16_t highest = 0;
    uint16_t legacy[kMaxLegacyRenames];
    size_t legacyCount = 0;
    fs::File root = SD.open("/");
    if (root) {
        for (fs::File f = root.openNextFile(); f; f = root.openNextFile()) {
            uint16_t serial = logFileSerial(f.name());
            if (serial == 0) {
                serial = legacyLogFileSerial(f.name());
                if (serial != 0 && legacyCount < kMaxLegacyRenames) legacy[legacyCount++] = serial;
            }
            if (serial > highest) highest = serial;
            f.close();
        }
        root.close();
    }
    for (size_t i = 0; i < legacyCount; ++i) {
        char from[sizeof("/layerwand_0000.log")];
        char to[kLogFileNameSize + 1] = "/";
        snprintf(from, sizeof(from), "/layerwand_%04u.log", static_cast<unsigned>(legacy[i]));
        if (!logFileName(legacy[i], to + 1, sizeof(to) - 1)) continue;
        if (SD.exists(to)) continue;
        if (SD.rename(from, to)) ++_c.legacyRenamed;
    }
    char path[kLogFileNameSize + 1] = "/";
    if (!logFileName(static_cast<uint16_t>(highest + 1), path + 1, sizeof(path) - 1)) {
        ++_c.writeErrors; // the card already holds hunter_9999.log
        closeAndUnmount();
        return false;
    }
    _file = SD.open(path, FILE_WRITE);
    if (!_file) {
        ++_c.writeErrors;
        closeAndUnmount();
        return false;
    }
    _c.fileOpen = true;
    ++_c.files;
    copyText(_c.fileName, sizeof(_c.fileName), path + 1);

    SdRecord boot;
    boot.kind = SdRecordKind::Boot;
    boot.uptimeMs = nowMs;
    copyText(boot.text, sizeof(boot.text), resetReason);
    char line[kLineSize];
    const size_t n = formatSdRecord(boot, bootCount, "", line, sizeof(line));
    if (n == 0 || !writeLine(kSdLogHeader, strlen(kSdLogHeader)) || !writeLine(line, n)) {
        closeAndUnmount();
        return false;
    }
    _file.flush();
    return true;
}

bool C5SdLog::writeLine(const char *line, size_t length)
{
    if (_file.write(reinterpret_cast<const uint8_t *>(line), length) != length) {
        ++_c.writeErrors;
        return false;
    }
    ++_c.lines;
    return true;
}

bool C5SdLog::writeQueued(uint32_t bootCount, uint16_t maxLines)
{
    char line[kLineSize];
    SdRecord r;
    for (uint16_t i = 0; i < maxLines && pop(r); ++i) {
        const char *name = r.kind == SdRecordKind::Event ? recon::detectorName(r.event.detector) : "";
        const size_t n = formatSdRecord(r, bootCount, name, line, sizeof(line));
        if (n == 0) {
            // Cannot happen at kLineSize (the longest line, every text byte
            // escaped, is about 350 bytes), but never silently.
            ++_c.unformatted;
            continue;
        }
        if (!writeLine(line, n)) return false;
    }
    return true;
}

void C5SdLog::closeAndUnmount()
{
    if (_c.fileOpen) {
        _file.close();
        _c.fileOpen = false;
    }
    SD.end();
    _c.mounted = false;
    startBus();
    holdCardDeselected();
}

} // namespace tdongle_c5
} // namespace layertime

#endif
