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

// The Recon stage log on the T-Dongle-C5: core's ReconStageLog
// (src/core/logic/ReconStageLog) over a ring in PSRAM, stamped with
// esp_timer_get_time(), behind a spinlock because the Wi-Fi driver task,
// the NimBLE host task and the loop task all record. Measurement
// instrumentation for the Slice 1 Increment 2A no-link baseline; nothing
// in the product path depends on it.

#include <stdint.h>

#include "core/logic/ReconStageLog.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

namespace layertime {
namespace tdongle_c5 {

class C5StageLog {
public:
    // Allocates the ring in PSRAM. Returns false, and stays detached (the log
    // then only counts), if PSRAM could not supply it.
    bool begin(uint32_t capacity)
    {
        void *mem = heap_caps_malloc(sizeof(recon::StageRecord) * capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (mem == nullptr) return false;
        _log.attach(static_cast<recon::StageRecord *>(mem), capacity);
        return true;
    }

    // Safe from any task; not from an interrupt.
    void record(recon::Stage stage, uint8_t a8 = 0, uint16_t a16 = 0, uint32_t a32 = 0)
    {
        const int64_t now = esp_timer_get_time();
        portENTER_CRITICAL(&_mux);
        _log.record(stage, now, a8, a16, a32);
        portEXIT_CRITICAL(&_mux);
    }

    // Reads are for the serial dump, from the loop task, with the radios
    // stopped; they take the lock per record all the same.
    uint32_t count()
    {
        portENTER_CRITICAL(&_mux);
        const uint32_t n = _log.count();
        portEXIT_CRITICAL(&_mux);
        return n;
    }
    recon::StageRecord at(uint32_t i)
    {
        portENTER_CRITICAL(&_mux);
        const recon::StageRecord r = _log.at(i);
        portEXIT_CRITICAL(&_mux);
        return r;
    }
    uint32_t total() { return _log.total(); }
    uint32_t lost() { return _log.lost(); }
    uint32_t capacity() { return _log.capacity(); }
    void clear()
    {
        portENTER_CRITICAL(&_mux);
        _log.clear();
        portEXIT_CRITICAL(&_mux);
    }

private:
    recon::ReconStageLog _log;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};

} // namespace tdongle_c5
} // namespace layertime
