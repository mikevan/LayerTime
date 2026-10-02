// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
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

// The Recon stage log: fixed-size records of when each stage of the Recon
// pipeline ran, kept in a ring the platform supplies, for the Slice 1
// Increment 2A no-link baseline on the T-Dongle-C5.
//
// A record is one stage, the platform's microsecond clock at that moment,
// and up to three small arguments whose meaning depends on the stage. The
// ring overwrites its oldest record when full and counts what it overwrote,
// so the evidence says when it is incomplete. Nothing here touches a radio,
// a clock, or memory allocation: the platform owns the buffer (on the C5,
// PSRAM) and the clock (esp_timer_get_time), and serialises the calls when
// more than one task records.
//
// This is measurement instrumentation, not product state. It is not part
// of the contract and nothing in core reads it.

#include <stddef.h>
#include <stdint.h>

namespace layertime {
namespace recon {

// Numeric values appear in the serial dump; append only.
enum class Stage : uint8_t {
    None = 0,
    RunStart = 1,        // a32: run mode
    RunEnd = 2,          // a32: run mode
    WifiStart = 3,       // promiscuous receive requested on channel 1; a32: 1 if it came up
    WifiStop = 4,        // a8: channel it was on
    WifiHop = 5,         // a8: channel now selected
    BleScanStart = 6,    // a8: selection scanned for; a32: duration ms
    BleScanEnd = 7,      // the stack reported the scan finished; a8: selection; a32: adverts so far
    WifiFrame = 8,       // first frame after a hop or start: a8 channel, a32 frames so far
    BleAdvert = 9,       // first advert after a scan start: a8 selection, a32 adverts so far
    Candidate = 10,      // a8: detector; a16: rssi as int8; a32: source kind
    EventRecorded = 11,  // the core sink returned; a8: detector; a32: microseconds it took
    PhaseChange = 12,    // a8: selected; a16: active; a32: monitoring | resting<<1 | enabled<<2
    Heap = 13,           // a32: free internal heap; a16: largest free block / 1024; a8: free PSRAM / MiB
    PollLate = 14,       // a32: loop period ms that fell in the top histogram bucket
};

struct StageRecord {
    Stage stage = Stage::None;
    uint8_t a8 = 0;
    uint16_t a16 = 0;
    uint32_t a32 = 0;
    int64_t tUs = 0;
};

class ReconStageLog {
public:
    // The ring lives in `buffer`, which must hold `capacity` records and
    // outlive this object. A null buffer or zero capacity leaves the log
    // detached: record() then only counts.
    void attach(StageRecord *buffer, uint32_t capacity);

    void record(Stage stage, int64_t tUs, uint8_t a8 = 0, uint16_t a16 = 0, uint32_t a32 = 0);
    void clear();

    // Records currently held, oldest first.
    uint32_t count() const { return _count; }
    const StageRecord &at(uint32_t i) const;
    // Records ever offered to record(), and those overwritten or dropped
    // because the ring was full or detached.
    uint32_t total() const { return _total; }
    uint32_t lost() const { return _lost; }
    uint32_t capacity() const { return _capacity; }

private:
    StageRecord *_buffer = nullptr;
    uint32_t _capacity = 0;
    uint32_t _head = 0; // next slot to write
    uint32_t _count = 0;
    uint32_t _total = 0;
    uint32_t _lost = 0;
};

// Loop-period and hop-interval evidence without a threshold: a histogram
// of intervals in fixed buckets, plus the minimum, maximum and sum, so the
// mean can be recovered. Buckets are upper bounds in milliseconds.
class IntervalHistogram {
public:
    static constexpr uint8_t kBuckets = 8;
    static const uint32_t kUpperMs[kBuckets]; // 5, 10, 20, 50, 100, 250, 1000, and above

    void add(uint32_t intervalMs);
    void clear();
    uint32_t count() const { return _count; }
    uint32_t bucket(uint8_t i) const { return i < kBuckets ? _bucket[i] : 0; }
    uint32_t minMs() const { return _count ? _min : 0; }
    uint32_t maxMs() const { return _max; }
    uint64_t sumMs() const { return _sum; }

private:
    uint32_t _bucket[kBuckets] = {0};
    uint32_t _count = 0;
    uint32_t _min = 0;
    uint32_t _max = 0;
    uint64_t _sum = 0;
};

} // namespace recon
} // namespace layertime
