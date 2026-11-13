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

// Port: where Recon candidates come from.
//
// A monitor source owns ACQUISITION only: starting, stopping, and configuring
// the radios, and handing each candidate it finds to core. It does not own
// the event history. That lives in core (logic/MonitorEventLog) and is
// cleared by the ReconClearEvents command, never by the source.

#include <stdint.h>

#include "../logic/ReconCandidate.h"
#include "../model/MonitorEvent.h"

namespace layertime {

// What the source is doing right now.
struct AcquisitionStatus {
    ReconTarget selected = ReconTarget::None;
    ReconTarget active = ReconTarget::None;
    bool monitoring = false;
    bool earlyWarningEnabled = false;
    bool earlyWarningResting = false;
};

class MonitorSource {
public:
    virtual ~MonitorSource() = default;

    // Starts manual monitoring for a selection. None stops it.
    virtual void start(ReconTarget target) = 0;
    // Leaves manual monitoring. The early-warning sweep resumes if enabled.
    virtual void stopManual() = 0;
    // Forgets the detectors' own tracking state (burst counters, per-BSSID
    // SSID sets). This is an acquisition-side reset. It does not touch the
    // event history.
    virtual void resetDetectorState() = 0;
    virtual void setEarlyWarningEnabled(bool enabled) = 0;
    // Radio scheduling. Called once per application tick.
    virtual void poll() = 0;
    virtual AcquisitionStatus acquisition() const = 0;
    // Where candidates go. May be called from a radio callback task.
    virtual void setCandidateSink(recon::CandidateSink sink, void *context) = 0;
};

} // namespace layertime
