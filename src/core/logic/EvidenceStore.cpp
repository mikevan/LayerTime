// LayerTime - counter-intrusion and resilient-communications firmware.
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

#include "EvidenceStore.h"

#include <string.h>

namespace layertime {
namespace evidence {

namespace {

constexpr uint8_t kTypeMgmt = 0;
constexpr uint8_t kTypeCtrl = 1;
constexpr uint8_t kTypeData = 2;
constexpr uint8_t kSubBeacon = 8;
constexpr uint8_t kSubProbeResp = 5;
constexpr uint16_t kHeader = 24;

uint8_t frameType(const uint8_t *f) { return (f[0] >> 2) & 0x03; }
uint8_t frameSubtype(const uint8_t *f) { return f[0] >> 4; }
bool same(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }
bool group(const uint8_t *a) { return (a[0] & 0x01) != 0; }
bool isBeaconLike(const uint8_t *f, uint16_t len)
{
    return len >= kHeader && frameType(f) == kTypeMgmt &&
           (frameSubtype(f) == kSubBeacon || frameSubtype(f) == kSubProbeResp);
}

// The SSID element of a beacon or probe response: element 0, after the
// 12 fixed bytes (timestamp, interval, capabilities).
bool ssidOf(const uint8_t *f, uint16_t len, const uint8_t *&ssid, uint8_t &ssidLen)
{
    if (!isBeaconLike(f, len)) return false;
    uint16_t p = kHeader + 12;
    while (p + 2 <= len) {
        const uint8_t id = f[p], n = f[p + 1];
        if (p + 2 + n > len) return false;
        if (id == 0) {
            if (n > 32) return false;
            ssid = f + p + 2;
            ssidLen = n;
            return true;
        }
        p = static_cast<uint16_t>(p + 2 + n);
    }
    return false;
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

size_t EvidenceStore::poolBytes()
{
    return sizeof(Window) * kWindows + sizeof(Slot) * kSlotCount;
}

bool EvidenceStore::attach(void *pool, size_t bytes)
{
    _slots = nullptr;
    _windows = nullptr;
    _ringNext = 0;
    _ringCount = 0;
    if (pool == nullptr || bytes < poolBytes()) return false;
    _windows = static_cast<Window *>(pool);
    for (uint8_t i = 0; i < kWindows; ++i) _windows[i] = Window{};
    _slots = reinterpret_cast<Slot *>(static_cast<uint8_t *>(pool) + sizeof(Window) * kWindows);
    return true;
}

bool EvidenceStore::parseMac(const char *text, uint8_t out[6])
{
    if (text == nullptr) return false;
    for (int i = 0; i < 6; ++i) {
        const int hi = hexDigit(text[i * 3]);
        const int lo = hi < 0 ? -1 : hexDigit(text[i * 3 + 1]);
        if (hi < 0 || lo < 0) return false;
        const char sep = text[i * 3 + 2];
        if (i < 5 ? sep != ':' : sep != '\0') return false;
        out[i] = static_cast<uint8_t>(hi * 16 + lo);
    }
    return true;
}

bool EvidenceStore::collecting(const Window &w, uint32_t nowMs) const
{
    return w.eventId != 0 && static_cast<int32_t>(w.untilMs - nowMs) > 0;
}

uint32_t EvidenceStore::followMsFor(uint8_t holdSec)
{
    const uint32_t holdMs = static_cast<uint32_t>(holdSec) * 1000u;
    return holdMs > kFollowMs ? holdMs : kFollowMs;
}

bool EvidenceStore::extendWindow(uint32_t eventId, uint32_t nowMs, uint32_t followMs)
{
    if (_windows == nullptr || eventId == 0) return false;
    for (uint8_t i = 0; i < kWindows; ++i) {
        Window &w = _windows[i];
        if (w.eventId != eventId) continue;
        const uint32_t until = nowMs + followMs;
        if (static_cast<int32_t>(until - w.untilMs) > 0) w.untilMs = until;
        return true;
    }
    return false;
}

const EvidenceStore::Window *EvidenceStore::find(uint32_t eventId) const
{
    if (_windows == nullptr || eventId == 0) return nullptr;
    for (uint8_t i = 0; i < kWindows; ++i)
        if (_windows[i].eventId == eventId) return &_windows[i];
    return nullptr;
}

// Learns the network name from the BSSID's own beacons, and the stations
// that exchange data with the BSSID.
void EvidenceStore::learnFrom(Window &w, const uint8_t *f, uint16_t len)
{
    if (len < kHeader) return;
    const uint8_t *a1 = f + 4, *a2 = f + 10, *a3 = f + 16;
    if (!w.haveSsid && w.haveBssid && isBeaconLike(f, len) && same(a2, w.bssid)) {
        const uint8_t *ssid = nullptr;
        uint8_t n = 0;
        if (ssidOf(f, len, ssid, n) && n > 0) {
            memcpy(w.ssid, ssid, n);
            w.ssidLen = n;
            w.haveSsid = true;
        }
    }
    if (frameType(f) != kTypeData || !w.haveBssid) return;
    const bool toDs = (f[1] & 0x01) != 0, fromDs = (f[1] & 0x02) != 0;
    const uint8_t *bssid = nullptr, *station = nullptr;
    if (toDs && !fromDs) { bssid = a1; station = a2; }
    else if (fromDs && !toDs) { bssid = a2; station = a1; }
    else if (!toDs && !fromDs) { bssid = a3; station = same(a2, a3) ? a1 : a2; }
    if (bssid == nullptr || !same(bssid, w.bssid) || group(station) || same(station, w.subject) || same(station, w.bssid)) return;
    for (uint8_t i = 0; i < w.stationCount; ++i)
        if (same(w.stations[i], station)) return;
    if (w.stationCount < kStations) memcpy(w.stations[w.stationCount++], station, 6);
}

bool EvidenceStore::member(Window &w, const uint8_t *f, uint16_t len, bool learn)
{
    if (len < kHeader || frameType(f) == kTypeCtrl) return false;
    if (learn) learnFrom(w, f, len);
    const uint8_t *addrs[3] = {f + 4, f + 10, f + 16};
    for (const uint8_t *a : addrs) {
        if (same(a, w.subject)) return true;
        if (w.haveBssid && same(a, w.bssid)) return true;
        for (uint8_t i = 0; i < w.stationCount; ++i)
            if (same(a, w.stations[i])) return true;
    }
    // A clone: someone else's beacon carrying this network's name.
    if (w.haveSsid) {
        const uint8_t *ssid = nullptr;
        uint8_t n = 0;
        if (ssidOf(f, len, ssid, n) && n == w.ssidLen && memcmp(ssid, w.ssid, n) == 0) return true;
    }
    return false;
}

// Beacon thinning: the first few, then one a second.
bool EvidenceStore::admit(Window &w, const uint8_t *f, uint16_t len, uint32_t atMs)
{
    if (!isBeaconLike(f, len)) return true;
    if (w.beaconsKept < kBeaconsFirst || static_cast<uint32_t>(atMs - w.lastBeaconMs) >= kBeaconEveryMs) {
        ++w.beaconsKept;
        w.lastBeaconMs = atMs;
        return true;
    }
    return false;
}

void EvidenceStore::append(uint8_t wi, const Slot &src, uint8_t extraFlags)
{
    Window &w = _windows[wi];
    ++w.seen;
    if (w.retained >= kWindowFrames) {
        if (w.dropped < 0xFFFF) ++w.dropped;
        return;
    }
    Slot *dst = windowSlot(wi, w.retained++);
    dst->meta = src.meta;
    dst->meta.flags = static_cast<uint8_t>(dst->meta.flags | extraFlags);
    memcpy(dst->bytes, src.bytes, src.meta.captured);
    if (src.meta.flags & kFlagTruncated) ++w.truncated;
}

void EvidenceStore::noteFrame(const uint8_t *frame, uint16_t len, int8_t rssi, uint8_t channel, uint32_t nowMs)
{
    if (_slots == nullptr || frame == nullptr || len == 0) return;
    Slot *s = ringSlot(_ringNext);
    s->meta.atMs = nowMs;
    s->meta.onAir = len;
    s->meta.captured = len > kMaxFrameBytes ? kMaxFrameBytes : len;
    s->meta.channel = channel;
    s->meta.rssi = rssi;
    s->meta.flags = len > kMaxFrameBytes ? kFlagTruncated : 0;
    memcpy(s->bytes, frame, s->meta.captured);
    _ringNext = static_cast<uint16_t>((_ringNext + 1) % kRingFrames);
    if (_ringCount < kRingFrames) ++_ringCount;

    for (uint8_t i = 0; i < kWindows; ++i) {
        Window &w = _windows[i];
        if (!collecting(w, nowMs)) continue;
        if (!member(w, s->bytes, s->meta.captured, true)) continue;
        if (!admit(w, s->bytes, s->meta.captured, nowMs)) continue;
        append(i, *s, 0);
    }
}

bool EvidenceStore::openWindow(uint32_t eventId, const uint8_t subject[6], uint32_t nowMs, uint32_t followMs)
{
    if (_slots == nullptr || _ringCount == 0 || eventId == 0) return false;
    // A free window, else the oldest.
    uint8_t wi = 0;
    bool freeFound = false;
    for (uint8_t i = 0; i < kWindows; ++i) {
        if (_windows[i].eventId == 0) { wi = i; freeFound = true; break; }
    }
    if (!freeFound) {
        for (uint8_t i = 1; i < kWindows; ++i)
            if (static_cast<int32_t>(_windows[i].openMs - _windows[wi].openMs) < 0) wi = i;
    }
    Window &w = _windows[wi];
    if (w.eventId != 0 && w.retained > 0 && !w.saved) ++_lostUnsaved;
    w = Window{};
    w.eventId = eventId;
    w.openMs = nowMs;
    w.untilMs = nowMs + followMs;
    memcpy(w.subject, subject, 6);

    const uint16_t newest = static_cast<uint16_t>((_ringNext + kRingFrames - 1) % kRingFrames);
    const Slot &trig = *ringSlot(newest);
    w.triggerMs = trig.meta.atMs;
    if (trig.meta.captured >= kHeader && frameType(trig.bytes) == kTypeMgmt) {
        memcpy(w.bssid, trig.bytes + 16, 6);
        w.haveBssid = true;
    } else if (trig.meta.captured >= kHeader) {
        // Not a management frame: treat the transmitter as the network to follow.
        memcpy(w.bssid, trig.bytes + 10, 6);
        w.haveBssid = true;
    }

    // Seed from the ring: the newest matching frames before the trigger,
    // within kSeedLookbackMs, at most kSeedFrames, at most kBeaconsFirst beacons.
    // Learning runs first over the lookback, so the network name and stations
    // are known before matching.
    uint16_t picks[kSeedFrames];
    uint8_t pickCount = 0, beaconPicks = 0;
    for (uint16_t k = 1; k < _ringCount; ++k) {
        const uint16_t idx = static_cast<uint16_t>((newest + kRingFrames - k) % kRingFrames);
        const Slot &s = *ringSlot(idx);
        if (static_cast<uint32_t>(w.triggerMs - s.meta.atMs) > kSeedLookbackMs) break;
        learnFrom(w, s.bytes, s.meta.captured);
    }
    learnFrom(w, trig.bytes, trig.meta.captured);
    for (uint16_t k = 1; k < _ringCount && pickCount < kSeedFrames; ++k) {
        const uint16_t idx = static_cast<uint16_t>((newest + kRingFrames - k) % kRingFrames);
        const Slot &s = *ringSlot(idx);
        if (static_cast<uint32_t>(w.triggerMs - s.meta.atMs) > kSeedLookbackMs) break;
        if (!member(w, s.bytes, s.meta.captured, false)) continue;
        if (isBeaconLike(s.bytes, s.meta.captured)) {
            if (beaconPicks >= kBeaconsFirst) continue;
            ++beaconPicks;
        }
        picks[pickCount++] = idx;
    }
    for (int8_t p = static_cast<int8_t>(pickCount) - 1; p >= 0; --p) {
        const Slot &s = *ringSlot(picks[p]);
        if (isBeaconLike(s.bytes, s.meta.captured)) { ++w.beaconsKept; w.lastBeaconMs = s.meta.atMs; }
        append(wi, s, 0);
    }
    append(wi, trig, kFlagTrigger);
    return true;
}

bool EvidenceStore::nextToSave(uint32_t nowMs, uint32_t &eventId) const
{
    if (_windows == nullptr) return false;
    const Window *best = nullptr;
    for (uint8_t i = 0; i < kWindows; ++i) {
        const Window &w = _windows[i];
        if (w.eventId == 0 || collecting(w, nowMs)) continue;
        if (w.saved && w.seen == w.savedSeen) continue;
        if (best == nullptr || static_cast<int32_t>(w.openMs - best->openMs) < 0) best = &w;
    }
    if (best == nullptr) return false;
    eventId = best->eventId;
    return true;
}

void EvidenceStore::markSaved(uint32_t eventId, uint16_t seen)
{
    if (_windows == nullptr || eventId == 0) return;
    for (uint8_t i = 0; i < kWindows; ++i) {
        Window &w = _windows[i];
        if (w.eventId != eventId) continue;
        w.saved = true;
        w.savedSeen = seen;
        return;
    }
}

bool EvidenceStore::info(uint32_t eventId, uint32_t nowMs, WindowInfo &out) const
{
    const Window *w = find(eventId);
    if (w == nullptr) return false;
    out.eventId = w->eventId;
    out.seen = w->seen;
    out.retained = w->retained;
    out.dropped = w->dropped;
    out.truncated = w->truncated;
    out.collecting = collecting(*w, nowMs);
    return true;
}

bool EvidenceStore::frameMeta(uint32_t eventId, uint8_t index, FrameMeta &out, int32_t &offsetMs) const
{
    const Window *w = find(eventId);
    if (w == nullptr || index >= w->retained) return false;
    const Slot *s = windowSlot(static_cast<uint8_t>(w - _windows), index);
    out = s->meta;
    offsetMs = static_cast<int32_t>(s->meta.atMs - w->triggerMs);
    return true;
}

size_t EvidenceStore::copyBytes(uint32_t eventId, uint8_t index, uint16_t offset, uint8_t *out, size_t max) const
{
    const Window *w = find(eventId);
    if (w == nullptr || index >= w->retained || out == nullptr) return 0;
    const Slot *s = windowSlot(static_cast<uint8_t>(w - _windows), index);
    if (offset >= s->meta.captured) return 0;
    size_t n = s->meta.captured - offset;
    if (n > max) n = max;
    memcpy(out, s->bytes + offset, n);
    return n;
}

} // namespace evidence
} // namespace layertime
