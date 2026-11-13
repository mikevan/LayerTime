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

#include "ReconStageLog.h"

namespace layertime {
namespace recon {

void ReconStageLog::attach(StageRecord *buffer, uint32_t capacity)
{
    _buffer = capacity > 0 ? buffer : nullptr;
    _capacity = _buffer ? capacity : 0;
    clear();
}

void ReconStageLog::clear()
{
    _head = 0;
    _count = 0;
    _total = 0;
    _lost = 0;
}

void ReconStageLog::record(Stage stage, int64_t tUs, uint8_t a8, uint16_t a16, uint32_t a32)
{
    ++_total;
    if (_buffer == nullptr) {
        ++_lost;
        return;
    }
    StageRecord &r = _buffer[_head];
    r.stage = stage;
    r.a8 = a8;
    r.a16 = a16;
    r.a32 = a32;
    r.tUs = tUs;
    _head = (_head + 1) % _capacity;
    if (_count < _capacity) {
        ++_count;
    } else {
        ++_lost; // the oldest record was overwritten
    }
}

const StageRecord &ReconStageLog::at(uint32_t i) const
{
    static const StageRecord kEmpty;
    if (_buffer == nullptr || i >= _count) return kEmpty;
    // The oldest record sits at head - count (mod capacity).
    const uint32_t oldest = (_head + _capacity - _count) % _capacity;
    return _buffer[(oldest + i) % _capacity];
}

const uint32_t IntervalHistogram::kUpperMs[IntervalHistogram::kBuckets] = {5, 10, 20, 50, 100, 250, 1000, 0xFFFFFFFFu};

void IntervalHistogram::add(uint32_t intervalMs)
{
    for (uint8_t i = 0; i < kBuckets; ++i) {
        if (intervalMs <= kUpperMs[i]) {
            ++_bucket[i];
            break;
        }
    }
    if (_count == 0 || intervalMs < _min) _min = intervalMs;
    if (intervalMs > _max) _max = intervalMs;
    _sum += intervalMs;
    ++_count;
}

void IntervalHistogram::clear()
{
    for (uint8_t i = 0; i < kBuckets; ++i) _bucket[i] = 0;
    _count = 0;
    _min = 0;
    _max = 0;
    _sum = 0;
}

} // namespace recon
} // namespace layertime
