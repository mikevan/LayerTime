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

#pragma once

// The frames behind each Wi-Fi detection (Link 0.2 evidence, slice 2,
// 2026-11-04). Sizes come from the Phase 0 measurements
// (claude/link_0_2_phase0_results_20261104.md): frames up to 953 bytes seen,
// so a 1,024-byte cap; copying a frame costs about 8 us against 69 us of
// classification; PSRAM has 7 MB free, so the whole store (about 2.4 MB)
// lives there.
//
// Two parts:
//   * A ring of the last kRingFrames frames heard, whatever they were. When a
//     detection opens a window, the window starts with the frames from the
//     ring that belong to it (the access point's beacons just before the
//     attack), then the triggering frame.
//   * Up to kWindows windows, one per detection, each up to kWindowFrames
//     frames. A window keeps collecting matching frames until its deadline:
//     followMsFor(evidence hold) after it opens, pushed out again by each
//     repeat sighting of the same detection (extendWindow). Collecting costs
//     no radio time, so a window never collects for less than kFollowMs even
//     when the hold is shorter or Off; the channel hold is what costs the
//     sweep, and only the hold follows the setting below 10 s.
//
// A frame belongs to a window when any of its three addresses is the
// detection's transmitter, the BSSID of the triggering frame, or a station
// seen talking to that BSSID; or when it is a beacon or probe response that
// carries the same network name as that BSSID (a clone). Beacons are thinned
// to the first kBeaconsFirst, then one per kBeaconEveryMs, so a 10-second
// window is not all beacons.
//
// Pure: no locks, no allocation. The platform hands in the memory (attach),
// and the window bookkeeping lives there too, so the store costs internal RAM
// only for a few counters (the Hunter has about 18 KB of internal RAM free).
// The platform serializes every call (the Wi-Fi task writes, the Link task
// reads).
// Every count is reported, so evidence is never silently incomplete.

#include <stddef.h>
#include <stdint.h>

namespace layertime {
namespace evidence {

constexpr uint16_t kMaxFrameBytes = 1024;
constexpr uint16_t kRingFrames = 256;
constexpr uint8_t kWindows = 32;
constexpr uint8_t kWindowFrames = 64;
constexpr uint8_t kSeedFrames = 16;
constexpr uint32_t kSeedLookbackMs = 2000;
constexpr uint32_t kFollowMs = 10000;
constexpr uint8_t kBeaconsFirst = 4;
constexpr uint32_t kBeaconEveryMs = 1000;
constexpr uint8_t kStations = 8;

// FrameMeta.flags
constexpr uint8_t kFlagTruncated = 0x01;
constexpr uint8_t kFlagFcsPresent = 0x02; // never set: the C5's trailing 4 bytes are stripped
constexpr uint8_t kFlagTrigger = 0x04;

struct FrameMeta {
    uint32_t atMs = 0;
    uint16_t onAir = 0;     // length as heard (FCS already stripped)
    uint16_t captured = 0;  // bytes kept, at most kMaxFrameBytes
    uint8_t channel = 0;
    int8_t rssi = 0;
    uint8_t flags = 0;
};

struct WindowInfo {
    uint32_t eventId = 0;
    uint16_t seen = 0;      // matching frames heard (after beacon thinning)
    uint8_t retained = 0;   // frames kept
    uint16_t dropped = 0;   // matching frames not kept: the window was full
    uint8_t truncated = 0;  // kept frames cut at kMaxFrameBytes
    bool collecting = false;
};

class EvidenceStore {
public:
    // Bytes attach() needs.
    static size_t poolBytes();

    // Hands the store its memory. False when it is too small; the store then
    // keeps nothing and every query answers "no window".
    bool attach(void *pool, size_t bytes);
    bool attached() const { return _slots != nullptr; }

    // Every Wi-Fi frame, before it is classified. `len` excludes the FCS.
    void noteFrame(const uint8_t *frame, uint16_t len, int8_t rssi, uint8_t channel, uint32_t nowMs);

    // A new detection: opens its window, triggered by the newest frame in the
    // ring (the one being classified when the detection was made). `subject`
    // is the detection's transmitter address. The window collects for
    // `followMs` from `nowMs`. False when nothing is attached or the ring is
    // empty.
    bool openWindow(uint32_t eventId, const uint8_t subject[6], uint32_t nowMs, uint32_t followMs = kFollowMs);

    // A repeat sighting of the detection behind `eventId`: the window collects
    // until at least `nowMs + followMs`, reopening a window whose deadline had
    // passed. A later deadline already set is kept. False when the window is
    // gone (never opened, or reused for a newer detection).
    bool extendWindow(uint32_t eventId, uint32_t nowMs, uint32_t followMs);

    // Saving windows to the card (2026-11-04). A window needs saving once it
    // has stopped collecting and has heard something since it was last saved
    // (never saved, or revived by a repeat sighting and heard more). Gives the
    // oldest such window; false when none does.
    bool nextToSave(uint32_t nowMs, uint32_t &eventId) const;
    // The window behind `eventId` was written when it had heard `seen` frames
    // (WindowInfo.seen from the copy that was written), so a frame heard after
    // the copy still makes it due again.
    void markSaved(uint32_t eventId, uint16_t seen);
    // Windows reused for a newer detection before they were ever saved, with
    // frames in them: evidence that never reached the card. Expected 0.
    uint32_t lostUnsaved() const { return _lostUnsaved; }

    // How long a window collects for an evidence hold of `holdSec` seconds:
    // the hold, but never less than kFollowMs.
    static uint32_t followMsFor(uint8_t holdSec);

    bool info(uint32_t eventId, uint32_t nowMs, WindowInfo &out) const;
    // One kept frame's meta and its time offset from the trigger, in ms.
    bool frameMeta(uint32_t eventId, uint8_t index, FrameMeta &out, int32_t &offsetMs) const;
    // Copies up to `max` bytes of a kept frame from `offset`. Returns the count;
    // 0 past the end or when there is no such frame.
    size_t copyBytes(uint32_t eventId, uint8_t index, uint16_t offset, uint8_t *out, size_t max) const;

    // Parses "aa:bb:cc:dd:ee:ff" (either case). False on anything else.
    static bool parseMac(const char *text, uint8_t out[6]);

private:
    struct Slot {
        FrameMeta meta;
        uint8_t bytes[kMaxFrameBytes];
    };
    struct Window {
        uint32_t eventId = 0;   // 0 = free
        uint32_t openMs = 0;
        uint32_t untilMs = 0;   // collecting while now is before this
        uint32_t triggerMs = 0;
        uint8_t subject[6] = {0};
        uint8_t bssid[6] = {0};
        bool haveBssid = false;
        uint8_t ssid[32] = {0};
        uint8_t ssidLen = 0;
        bool haveSsid = false;
        uint8_t stations[kStations][6] = {{0}};
        uint8_t stationCount = 0;
        uint8_t beaconsKept = 0;
        uint32_t lastBeaconMs = 0;
        uint16_t seen = 0;
        uint8_t retained = 0;
        uint16_t dropped = 0;
        uint8_t truncated = 0;
        bool saved = false;     // written to the card at least once
        uint16_t savedSeen = 0; // `seen` when it was last written
    };

    Slot *ringSlot(uint16_t i) const { return &_slots[i]; }
    Slot *windowSlot(uint8_t w, uint8_t i) const { return &_slots[kRingFrames + static_cast<size_t>(w) * kWindowFrames + i]; }
    static constexpr size_t kSlotCount = kRingFrames + static_cast<size_t>(kWindows) * kWindowFrames;
    const Window *find(uint32_t eventId) const;
    bool collecting(const Window &w, uint32_t nowMs) const;
    bool member(Window &w, const uint8_t *f, uint16_t len, bool learn);
    bool admit(Window &w, const uint8_t *f, uint16_t len, uint32_t atMs);
    void append(uint8_t w, const Slot &src, uint8_t extraFlags);
    void learnFrom(Window &w, const uint8_t *f, uint16_t len);

    Slot *_slots = nullptr;
    Window *_windows = nullptr;
    uint16_t _ringNext = 0;
    uint16_t _ringCount = 0;
    uint32_t _lostUnsaved = 0;
};

} // namespace evidence
} // namespace layertime
