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

// Canonical definition: contracts/models.md, "ReconState".

#include <stdint.h>

#include "MonitorEvent.h"

namespace layertime {

// What Recon is doing right now.
//
// The event list itself is deliberately NOT embedded here. The producer
// already owns that storage (on the T-Ultra, ReconStatus::detections).
// Copying it into a second array would double the RAM and create two copies
// that can disagree. Events are read through the monitor source instead.
struct ReconState {
    // Most events a producer keeps. When full, the oldest is dropped. Taken
    // from the T-Ultra's ReconStatus::MAX_DETECTIONS.
    static constexpr uint8_t kMaxEvents = 40;

    // What the user selected.
    ReconTarget selected = ReconTarget::None;
    // What is scanning at this instant. Differs from selected while a group
    // or the early-warning sweep rotates through its members.
    ReconTarget active = ReconTarget::None;

    bool monitoring = false;

    bool earlyWarningEnabled = false;
    // True during the early-warning sweep's rest period, when nothing is
    // being scanned. A client must show this rather than imply continuous
    // coverage.
    bool earlyWarningResting = false;

    // An alert has been raised and not yet acknowledged.
    bool alertPending = false;

    // eventId of the most recently created event. 0 means none yet.
    uint32_t lastEventId = 0;
    uint8_t eventCount = 0;
};

} // namespace layertime
