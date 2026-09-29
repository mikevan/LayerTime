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

// The LayerTime application. Screens issue LayerTimeCommands to it and read
// state back from it; the platform plugs its radios, alert hardware, log
// storage and position source in through the ports.
//
// Phase 0 Step 4 scope: Recon commands, the Recon event history, alert
// actuation, and navigation state. Mesh commands are answered Unsupported
// until the mesh services are moved behind ports.

#include <stdint.h>

#include "../logic/MonitorEventLog.h"
#include "../model/LayerTimeCommand.h"
#include "../model/NavigationState.h"
#include "../model/ReconState.h"
#include "../ports/AlertSink.h"
#include "../ports/EventLog.h"
#include "../ports/MonitorSource.h"
#include "../ports/NavigationSource.h"

namespace layertime {

// Any port may be left null. A command that needs a missing port answers
// Unsupported.
struct CorePorts {
    MonitorSource *monitor = nullptr;
    AlertSink *alerts = nullptr;
    EventLog *eventLog = nullptr;
    NavigationSource *navigation = nullptr;
};

class LayerTimeCore {
public:
    void attach(const CorePorts &ports);

    CommandResult execute(const LayerTimeCommand &command);

    // Once per application loop pass: raises the alert for a new alerting
    // event (at most once per event), then lets the monitor source schedule
    // its radios. That order is the order ReconService::poll() used.
    void tick(uint32_t nowMs);

    void setSleepMode(bool enabled) { _events.setSleepMode(enabled); }

    ReconState reconState() const;
    uint8_t eventCount() const { return _events.count(); }
    // Oldest first. i must be below eventCount().
    const MonitorEvent &event(uint8_t i) const { return _events.event(i); }

    void refreshNavigation();
    const NavigationState &navigation() const { return _navigation; }

private:
    static void candidateThunk(const recon::Candidate &candidate, void *self);

    CorePorts _ports;
    recon::MonitorEventLog _events;
    // eventId the alert was last raised for.
    uint32_t _alertRaisedFor = 0;
    NavigationState _navigation;
};

} // namespace layertime
