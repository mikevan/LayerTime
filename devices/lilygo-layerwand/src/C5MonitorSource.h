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

// MonitorSource on the T-Dongle-C5: core's ReconScheduler driving the
// C5ReconRadio. The schedule is core's; the six radio primitives behind it
// are the C5's.

#include "core/logic/ReconScheduler.h"
#include "core/ports/MonitorSource.h"
#include "C5ReconRadio.h"

#include <Arduino.h>

namespace layertime {
namespace tdongle_c5 {

class C5MonitorSource : public MonitorSource {
public:
    explicit C5MonitorSource(C5ReconRadio &radio) : _radio(radio), _scheduler(radio, clockThunk)
    {
        _radio.bindScheduler(&_scheduler);
    }

    void start(ReconTarget target) override { _scheduler.start(target); }
    void stopManual() override { _scheduler.stopManual(); }
    void resetDetectorState() override { _radio.resetDetectorState(); }
    void setEarlyWarningEnabled(bool enabled) override { _scheduler.setEarlyWarningEnabled(enabled); }
    void poll() override { _scheduler.poll(millis()); }
    AcquisitionStatus acquisition() const override { return _scheduler.status(); }
    void setCandidateSink(recon::CandidateSink sink, void *context) override
    {
        _radio.setCandidateSink(sink, context);
    }

    // Tears both radios down without touching the early-warning enable.
    void stop() { _scheduler.stop(); }
    const recon::ReconScheduler &scheduler() const { return _scheduler; }
    // For setup only: the channel plan, sweep mode, and random source.
    recon::ReconScheduler &scheduler() { return _scheduler; }

private:
    static uint32_t clockThunk() { return millis(); }

    C5ReconRadio &_radio;
    recon::ReconScheduler _scheduler;
};

} // namespace tdongle_c5
} // namespace layertime
