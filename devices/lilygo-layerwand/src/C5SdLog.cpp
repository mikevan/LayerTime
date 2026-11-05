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
    digitalWrite(pins::kSdCs, HIGH);
    pinMode(pins::kSdCs, OUTPUT);
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
    if (!_c.fileOpen || _reading || pcap == nullptr || length == 0) return false;
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
    if (!_c.fileOpen || _reading || entry == nullptr) return false;
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

bool C5SdLog::beginRead(const char *name, uint32_t &size, bool &usedBus)
{
    usedBus = false;
    size = 0;
    if (!_c.fileOpen || _reading || !cardNameServed(name)) return false;
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
    if (_reading) return false;
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

    uint16_t highest = 0;
    fs::File root = SD.open("/");
    if (root) {
        for (fs::File f = root.openNextFile(); f; f = root.openNextFile()) {
            const uint16_t serial = logFileSerial(f.name());
            if (serial > highest) highest = serial;
            f.close();
        }
        root.close();
    }
    char path[kLogFileNameSize + 1] = "/";
    if (!logFileName(static_cast<uint16_t>(highest + 1), path + 1, sizeof(path) - 1)) {
        ++_c.writeErrors; // the card already holds layerwand_9999.log
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
