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

#include "LayerTimeCore.h"

namespace layertime {

void LayerTimeCore::attach(const CorePorts &ports)
{
    _ports = ports;
    _events.setRecorder(ports.eventLog);
    if (_ports.monitor) _ports.monitor->setCandidateSink(candidateThunk, this);
}

void LayerTimeCore::candidateThunk(const recon::Candidate &candidate, void *self)
{
    static_cast<LayerTimeCore *>(self)->_events.add(candidate);
}

CommandResult LayerTimeCore::execute(const LayerTimeCommand &command)
{
    switch (command.type) {
    case CommandType::ReconStart:
        if (!_ports.monitor) return CommandResult::Unsupported;
        _ports.monitor->start(command.reconTarget.target);
        return CommandResult::Ok;

    case CommandType::ReconStop:
        if (!_ports.monitor) return CommandResult::Unsupported;
        _ports.monitor->stopManual();
        return CommandResult::Ok;

    case CommandType::ReconClearEvents:
        // Two separate things, as before the move: the application's event
        // history (owned here), and the detectors' own tracking state (owned
        // by the monitor source).
        _events.clear();
        if (_ports.monitor) _ports.monitor->resetDetectorState();
        return CommandResult::Ok;

    case CommandType::ReconAcknowledgeAlert:
        _events.acknowledgeAlert();
        return CommandResult::Ok;

    case CommandType::MeshSendText:
    case CommandType::MeshSendQuickMessage:
    case CommandType::MeshSetChannel:
    case CommandType::MeshRemoveChannel:
        // Mesh is not behind a port yet. The mesh screens still call their
        // services directly.
        return CommandResult::Unsupported;

    case CommandType::None:
    default:
        return CommandResult::InvalidArgument;
    }
}

void LayerTimeCore::tick(uint32_t nowMs)
{
    if (_events.alertPending() && _alertRaisedFor != _events.lastEventId()) {
        _alertRaisedFor = _events.lastEventId();
        if (_ports.alerts) {
            Alert alert;
            alert.kind = AlertKind::ReconDetection;
            alert.eventId = _alertRaisedFor;
            alert.raised.uptimeMs = nowMs;
            _ports.alerts->raise(alert);
        }
    }

    if (_ports.monitor) _ports.monitor->poll();
}

ReconState LayerTimeCore::reconState() const
{
    ReconState s;
    if (_ports.monitor) {
        const AcquisitionStatus a = _ports.monitor->acquisition();
        s.selected = a.selected;
        s.active = a.active;
        s.monitoring = a.monitoring;
        s.earlyWarningEnabled = a.earlyWarningEnabled;
        s.earlyWarningResting = a.earlyWarningResting;
    }
    s.alertPending = _events.alertPending();
    s.lastEventId = _events.lastEventId();
    s.eventCount = _events.count();
    return s;
}

void LayerTimeCore::refreshNavigation()
{
    if (_ports.navigation) _ports.navigation->read(_navigation);
}

} // namespace layertime
