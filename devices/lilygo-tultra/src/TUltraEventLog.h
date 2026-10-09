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

// EventLog on the T-Watch Ultra: one CSV row per new event on the SD card,
// when the Recon SD logging setting is on. Moved from
// WatchApp::logReconDetection in Phase 0 Step 4, output unchanged.

#include "core/ports/EventLog.h"
#include "model/TUltraSettings.h"
#include "model/WatchState.h"
#include "services/SdCardService.h"

namespace layertime {
namespace twatch_ultra {

class TUltraEventLog : public EventLog {
public:
    // The row is stamped from `state`, WatchApp's cached clock, refreshed
    // every 250 ms - not the clock at the moment of detection.
    TUltraEventLog(const TUltraSettings &settings, const WatchState &state, SdCardService &sdCard)
        : _settings(settings), _state(state), _sdCard(sdCard) {}

    void append(const MonitorEvent &event) override;

private:
    const TUltraSettings &_settings;
    const WatchState &_state;
    SdCardService &_sdCard;
};

} // namespace twatch_ultra
} // namespace layertime
