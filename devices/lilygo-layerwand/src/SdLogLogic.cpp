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

#include "SdLogLogic.h"

#include <stdio.h>
#include <string.h>

namespace layertime {
namespace tdongle_c5 {

// --- File names ----------------------------------------------------------

namespace {

const char kPrefix[] = "layerwand_";
const char kSuffix[] = ".log";

char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool matchesIgnoringCase(const char *text, const char *expected, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (lower(text[i]) != expected[i]) return false;
    }
    return true;
}

} // namespace

uint16_t logFileSerial(const char *name)
{
    if (name == nullptr) return 0;
    if (name[0] == '/') ++name;
    const size_t prefix = sizeof(kPrefix) - 1;
    const size_t suffix = sizeof(kSuffix) - 1;
    if (strlen(name) != prefix + 4 + suffix) return 0;
    if (!matchesIgnoringCase(name, kPrefix, prefix)) return 0;
    if (!matchesIgnoringCase(name + prefix + 4, kSuffix, suffix)) return 0;
    uint16_t serial = 0;
    for (size_t i = 0; i < 4; ++i) {
        const char c = name[prefix + i];
        if (c < '0' || c > '9') return 0;
        serial = static_cast<uint16_t>(serial * 10 + (c - '0'));
    }
    return serial;
}

bool logFileName(uint16_t serial, char *out, size_t size)
{
    if (serial == 0 || serial > kMaxLogSerial || out == nullptr || size < kLogFileNameSize) return false;
    snprintf(out, size, "layerwand_%04u.log", static_cast<unsigned>(serial));
    return true;
}

// --- Records -------------------------------------------------------------

void SdRecordRing::attach(SdRecord *storage, uint16_t capacity)
{
    _storage = storage;
    _capacity = storage == nullptr ? 0 : capacity;
    _head = 0;
    _count = 0;
    _dropped = 0;
}

bool SdRecordRing::push(const SdRecord &record)
{
    if (_count >= _capacity) {
        ++_dropped;
        return false;
    }
    _storage[(_head + _count) % _capacity] = record;
    ++_count;
    return true;
}

bool SdRecordRing::pop(SdRecord &out)
{
    if (_count == 0) return false;
    out = _storage[_head];
    _head = static_cast<uint16_t>((_head + 1) % _capacity);
    --_count;
    return true;
}

// --- CSV -----------------------------------------------------------------

const char kSdLogHeader[] =
    "boot,uptime_ms,record,event_id,detector,confidence,source,source_id,detail,rssi,channel,band,count\r\n";

size_t csvQuoted(const char *text, char *out, size_t size)
{
    if (out == nullptr || size == 0) return 0;
    size_t n = 0;
    auto put = [&](char c) -> bool {
        if (n + 1 >= size) return false; // keep room for the terminator
        out[n++] = c;
        return true;
    };
    if (!put('"')) return 0;
    for (const char *p = text == nullptr ? "" : text; *p != '\0'; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        bool ok;
        if (c == '"') {
            ok = put('"') && put('"');
        } else if (c == '\\') {
            ok = put('\\') && put('\\');
        } else if (c < 0x20 || c == 0x7F) {
            static const char kHex[] = "0123456789ABCDEF";
            ok = put('\\') && put('x') && put(kHex[c >> 4]) && put(kHex[c & 0x0F]);
        } else {
            ok = put(static_cast<char>(c));
        }
        if (!ok) {
            out[0] = '\0';
            return 0;
        }
    }
    if (!put('"')) {
        out[0] = '\0';
        return 0;
    }
    out[n] = '\0';
    return n;
}

namespace {

const char *recordName(SdRecordKind k)
{
    switch (k) {
    case SdRecordKind::Boot: return "boot";
    case SdRecordKind::Link: return "link";
    case SdRecordKind::Mode: return "mode";
    default: return "event";
    }
}

const char *confidenceName(Confidence c)
{
    switch (c) {
    case Confidence::Low: return "low";
    case Confidence::Medium: return "medium";
    default: return "high";
    }
}

const char *sourceName(SourceKind s)
{
    switch (s) {
    case SourceKind::Wifi: return "wifi";
    case SourceKind::Ble: return "ble";
    case SourceKind::Ieee802154: return "802.15.4";
    default: return "unknown";
    }
}

const char *bandName(Band b)
{
    switch (b) {
    case Band::Band2_4GHz: return "2.4GHz";
    case Band::Band5GHz: return "5GHz";
    default: return "";
    }
}

// Appends to out at *n; false when it does not fit.
bool append(char *out, size_t size, size_t *n, const char *text)
{
    const size_t len = strlen(text);
    if (*n + len + 1 > size) return false;
    memcpy(out + *n, text, len + 1);
    *n += len;
    return true;
}

bool appendQuoted(char *out, size_t size, size_t *n, const char *text)
{
    if (*n >= size) return false;
    const size_t len = csvQuoted(text, out + *n, size - *n);
    if (len == 0) return false;
    *n += len;
    return true;
}

} // namespace

size_t formatSdRecord(const SdRecord &r, uint32_t bootCount, const char *detectorName, char *out, size_t size)
{
    if (out == nullptr || size == 0) return 0;
    out[0] = '\0';
    size_t n = 0;
    char num[48];

    snprintf(num, sizeof(num), "%lu,%lu,", static_cast<unsigned long>(bootCount),
             static_cast<unsigned long>(r.uptimeMs));
    bool ok = append(out, size, &n, num) && appendQuoted(out, size, &n, recordName(r.kind));

    if (r.kind == SdRecordKind::Event) {
        const MonitorEvent &e = r.event;
        snprintf(num, sizeof(num), ",%lu,", static_cast<unsigned long>(e.eventId));
        ok = ok && append(out, size, &n, num) && appendQuoted(out, size, &n, detectorName) &&
             append(out, size, &n, ",") && appendQuoted(out, size, &n, confidenceName(e.confidence)) &&
             append(out, size, &n, ",") && appendQuoted(out, size, &n, sourceName(e.sourceKind)) &&
             append(out, size, &n, ",") && appendQuoted(out, size, &n, e.sourceId) &&
             append(out, size, &n, ",") && appendQuoted(out, size, &n, e.detail);
        snprintf(num, sizeof(num), ",%d,%u,", static_cast<int>(e.rssi), static_cast<unsigned>(e.channel));
        ok = ok && append(out, size, &n, num) && appendQuoted(out, size, &n, bandName(e.band));
        snprintf(num, sizeof(num), ",%lu\r\n", static_cast<unsigned long>(e.count));
        ok = ok && append(out, size, &n, num);
    } else {
        // event_id, detector, confidence, and source are empty; source_id
        // holds the watch's address on a link record; detail holds the text.
        ok = ok && append(out, size, &n, ",,,,,") && appendQuoted(out, size, &n, r.peer) &&
             append(out, size, &n, ",") && appendQuoted(out, size, &n, r.text) &&
             append(out, size, &n, ",,,,\r\n");
    }
    if (!ok) {
        out[0] = '\0';
        return 0;
    }
    return n;
}

} // namespace tdongle_c5
} // namespace layertime
