// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
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

// MonitorSource on the S3 Plus: S3PlusReconService behind the core's port,
// like the T-Ultra's TUltraMonitorSource.
//
// One difference, and the reason this is not a plain pass-through: poll().
// The core calls it from inside core.tick(), and S3PlusApp holds the
// event-log lock around core.tick() so alerts read a consistent history. The
// radios must never be driven with that lock held (EventLock.h), so poll()
// releases it for the radio work and takes it back before returning to the
// core.

#include "core/ports/MonitorSource.h"

#include "EventLock.h"
#include "S3PlusReconService.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusMonitorSource : public MonitorSource {
public:
    S3PlusMonitorSource(S3PlusReconService &recon, EventLock &lock) : _recon(recon), _lock(lock) {}

    void start(ReconTarget target) override { _recon.startDetector(target); }
    void stopManual() override { _recon.exitManualMode(); }
    void resetDetectorState() override { _recon.resetDetectorState(); }
    void setEarlyWarningEnabled(bool enabled) override { _recon.setEarlyWarningEnabled(enabled); }

    // Called by core.tick() with the event-log lock held.
    void poll() override
    {
        _lock.give();
        _recon.poll();
        _lock.take();
    }

    AcquisitionStatus acquisition() const override
    {
        const ReconStatus &s = _recon.status();
        AcquisitionStatus a;
        a.selected = s.detector;
        a.active = s.activeDetector;
        a.monitoring = s.monitoring;
        a.earlyWarningEnabled = s.earlyWarningEnabled;
        a.earlyWarningResting = s.earlyWarningResting;
        return a;
    }

    void setCandidateSink(recon::CandidateSink sink, void *context) override
    {
        _recon.setCandidateSink(sink, context);
    }

private:
    S3PlusReconService &_recon;
    EventLock &_lock;
};

} // namespace twatch_s3plus
} // namespace layertime
