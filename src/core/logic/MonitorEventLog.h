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

// The Recon event history. Moved out of ReconService (addDetection,
// clearDetections, acknowledgeAlert) in Phase 0 Step 4, behaviour unchanged:
//
//   * One record per (detector, sourceId). A repeat sighting updates rssi,
//     channel, lastSeen and count in place and keeps the strongest
//     confidence ever seen (AlertPolicy::onRepeatSighting).
//   * When full, the oldest record is dropped to make room.
//   * eventId comes from a serial that only a new record advances. clear()
//     does not reset it.
//   * A new record raises the alert per AlertPolicy::raisesOnNewRecord. A
//     repeat never does (the KNOWN DEFECT pinned in test_recon).
//   * clear() empties the history and drops a pending alert. It does not
//     touch acquisition. The detectors' own tracking state is reset by the
//     monitor source, separately (MonitorSource::resetDetectorState).
//
// Not thread-safe. On the T-Ultra add() runs on the Wi-Fi and NimBLE
// callback tasks while the UI reads, exactly as ReconService did before the
// move. That race is pre-existing and deliberately carried over unchanged.

#include <stdint.h>

#include "../model/MonitorEvent.h"
#include "../model/ReconState.h"
#include "../ports/EventLog.h"
#include "ReconCandidate.h"

namespace layertime {
namespace recon {

class MonitorEventLog {
public:
    static constexpr uint8_t kCapacity = ReconState::kMaxEvents;

    void add(const Candidate &candidate);
    void clear();
    void acknowledgeAlert() { _alertPending = false; }

    // While on, new records are still kept and counted but raise no alert.
    void setSleepMode(bool enabled) { _sleepMode = enabled; }
    // Receives each newly created record. Optional.
    void setRecorder(EventLog *recorder) { _recorder = recorder; }

    uint8_t count() const { return _count; }
    // Oldest first. i must be below count().
    const MonitorEvent &event(uint8_t i) const { return _events[i]; }
    bool alertPending() const { return _alertPending; }
    // eventId of the most recently created record; 0 before the first.
    uint32_t lastEventId() const { return _serial; }

private:
    MonitorEvent _events[kCapacity];
    uint8_t _count = 0;
    uint32_t _serial = 0;
    bool _alertPending = false;
    bool _sleepMode = false;
    EventLog *_recorder = nullptr;
};

} // namespace recon
} // namespace layertime
