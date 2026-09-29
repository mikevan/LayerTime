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

#include <string.h>

#include "../logic/QuickMessages.h"

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
        return sendMeshText(command.meshText.network, command.meshText.destination,
                            command.meshText.text, sizeof(command.meshText.text));

    case CommandType::MeshSendQuickMessage: {
        if (!meshTransport(command.meshQuick.network)) return CommandResult::Unsupported;
        const QuickMessage *m = mesh::findQuickMessage(command.meshQuick.quickMessageId);
        if (!m) return CommandResult::InvalidArgument;
        return sendMeshText(command.meshQuick.network, command.meshQuick.destination, m->text,
                            sizeof(m->text));
    }

    case CommandType::MeshSetChannel: {
        MeshTransport *t = meshTransport(MeshNetwork::Meshtastic);
        if (!t) return CommandResult::Unsupported;
        const MeshChannelArgs &a = command.meshChannel;
        if (!memchr(a.name, 0, sizeof(a.name)) || !memchr(a.key, 0, sizeof(a.key)))
            return CommandResult::InvalidArgument;
        return t->setChannel(a.index, a.name, a.key);
    }

    case CommandType::MeshRemoveChannel: {
        MeshTransport *t = meshTransport(MeshNetwork::Meshtastic);
        if (!t) return CommandResult::Unsupported;
        const CommandResult r = t->removeChannel(command.meshChannel.index);
        // The channel's conversation goes whatever the network answered, as
        // it always has on the T-Ultra.
        _conversations.removeChannel(command.meshChannel.index);
        return r;
    }

    case CommandType::SetClockFormat:
        _settings.use24Hour = command.setting.enabled;
        return CommandResult::Ok;

    case CommandType::SetUnits:
        _settings.metricUnits = command.setting.enabled;
        return CommandResult::Ok;

    case CommandType::SetSleepMode:
        setSleepMode(command.setting.enabled);
        return CommandResult::Ok;

    case CommandType::SetEarlyWarning:
        if (!_ports.monitor) return CommandResult::Unsupported;
        _settings.earlyWarningEnabled = command.setting.enabled;
        return CommandResult::Ok;

    case CommandType::MeshSetAdvertising:
        if (!meshTransport(command.setting.network)) return CommandResult::Unsupported;
        _settings.meshAdvertising[static_cast<uint8_t>(command.setting.network)] = command.setting.enabled;
        return CommandResult::Ok;

    case CommandType::MeshSetOwnName: {
        // Only Meshtastic takes a chosen name; MeshCore's comes from its key.
        if (command.setting.network != MeshNetwork::Meshtastic || !meshTransport(MeshNetwork::Meshtastic))
            return CommandResult::Unsupported;
        const char *name = command.setting.name;
        if (!memchr(name, 0, sizeof(command.setting.name))) return CommandResult::InvalidArgument;
        static_assert(sizeof(ApplicationSettings{}.meshtasticName) == SettingArgs::kNameSize,
                      "a name that fits the command fits the setting");
        memcpy(_settings.meshtasticName, name, sizeof(_settings.meshtasticName));
        return CommandResult::Ok;
    }

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

MeshTransport *LayerTimeCore::meshTransport(MeshNetwork network) const
{
    const uint8_t i = static_cast<uint8_t>(network);
    if (i >= kMeshNetworkCount) return nullptr;
    MeshTransport *t = _ports.mesh[i];
    return (t && t->network() == network) ? t : nullptr;
}

CommandResult LayerTimeCore::sendMeshText(MeshNetwork network, const MeshDestination &destination,
                                          const char *text, size_t textBufferSize)
{
    MeshTransport *t = meshTransport(network);
    if (!t) return CommandResult::Unsupported;

    if (destination.kind == MeshDestinationKind::Node) {
        MeshNetwork idNetwork;
        if (!meshIdWellFormed(destination.node) || !meshIdNetwork(destination.node.kind, idNetwork) ||
            idNetwork != network)
            return CommandResult::InvalidArgument;
    } else if (destination.kind != MeshDestinationKind::Channel) {
        return CommandResult::InvalidArgument;
    }

    // Text must be present and end inside its buffer.
    if (!text || text[0] == '\0' || !memchr(text, 0, textBufferSize)) return CommandResult::InvalidArgument;
    if (strlen(text) > MeshSendTextArgs::kMaxTextChars) return CommandResult::InvalidArgument;

    return t->sendText(destination, text);
}

void LayerTimeCore::refreshMesh()
{
    for (uint8_t i = 0; i < kMeshNetworkCount; ++i) {
        MeshNetworkStatus &s = _mesh.networks[i];
        MeshTransport *t = meshTransport(static_cast<MeshNetwork>(i));
        if (t) t->observeStatus(s);
        else s = MeshNetworkStatus{};
    }
}

void LayerTimeCore::setSleepMode(bool enabled)
{
    _settings.sleepModeEnabled = enabled;
    _events.setSleepMode(enabled);
}

void LayerTimeCore::loadSettings()
{
    if (_ports.settings) _ports.settings->load(_settings);
    _events.setSleepMode(_settings.sleepModeEnabled);
}

void LayerTimeCore::saveSettings()
{
    if (_ports.settings) _ports.settings->save(_settings);
}

const QuickMessage *LayerTimeCore::quickMessages(uint8_t &count) const
{
    return mesh::defaultQuickMessages(count);
}

} // namespace layertime
