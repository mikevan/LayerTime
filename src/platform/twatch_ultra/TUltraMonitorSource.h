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

// MonitorSource on the T-Watch Ultra: the existing ReconService, which does
// the Wi-Fi promiscuous capture, NimBLE scanning and radio scheduling.

#include "../../core/ports/MonitorSource.h"
#include "../../services/ReconService.h"

namespace layertime {
namespace twatch_ultra {

class TUltraMonitorSource : public MonitorSource {
public:
    explicit TUltraMonitorSource(ReconService &recon) : _recon(recon) {}

    void start(ReconTarget target) override { _recon.startDetector(target); }
    void stopManual() override { _recon.exitManualMode(); }
    void resetDetectorState() override { _recon.resetDetectorState(); }
    void setEarlyWarningEnabled(bool enabled) override { _recon.setEarlyWarningEnabled(enabled); }
    void poll() override { _recon.poll(); }
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
    ReconService &_recon;
};

} // namespace twatch_ultra
} // namespace layertime
