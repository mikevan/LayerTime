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

#include "MonitorEventLog.h"

#include <stdio.h>
#include <string.h>

#include "AlertPolicy.h"

namespace layertime {
namespace recon {

void MonitorEventLog::add(const Candidate &c)
{
    for (uint8_t i = 0; i < _count; ++i) {
        MonitorEvent &existing = _events[i];
        if (existing.detector == c.detector && strcmp(existing.sourceId, c.address) == 0) {
            existing.rssi = c.rssi;
            existing.channel = c.channel;
            existing.lastSeen.uptimeMs = c.atMs;
            const alert::RepeatOutcome outcome =
                alert::onRepeatSighting(existing.confidence, c.confidence);
            existing.confidence = outcome.confidence;
            if (outcome.raiseAlert) _alertPending = true;
            ++existing.count;
            return;
        }
    }

    uint8_t index = _count;
    if (index >= kCapacity) {
        memmove(&_events[0], &_events[1], sizeof(MonitorEvent) * (kCapacity - 1));
        index = kCapacity - 1;
    } else ++_count;

    MonitorEvent &entry = _events[index];
    entry = MonitorEvent{};
    entry.detector = c.detector;
    snprintf(entry.detail, sizeof(entry.detail), "%s", c.detail ? c.detail : "Activity detected");
    snprintf(entry.sourceId, sizeof(entry.sourceId), "%s", c.address ? c.address : "");
    entry.sourceKind = c.sourceKind;
    entry.band = c.band;
    entry.rssi = c.rssi;
    entry.channel = c.channel;
    entry.confidence = c.confidence;
    entry.lastSeen.uptimeMs = c.atMs;
    entry.eventId = ++_serial;
    if (alert::raisesOnNewRecord(c.confidence, _sleepMode)) {
        _alertPending = true;
    }

    if (_recorder != nullptr) {
        _recorder->append(entry);
    }
}

void MonitorEventLog::clear()
{
    _count = 0;
    _alertPending = false;
}

} // namespace recon
} // namespace layertime
