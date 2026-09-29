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

// MeshTransport on the T-Watch Ultra: the existing MeshCore and Meshtastic
// services, which share one SX1262. Which of them owns the radio is still
// decided by WatchApp from Settings; a transport only carries commands out
// and reports what its service observes.
//
// Every command is passed to the service as it stood before Phase 0 Step 5,
// so the service rejects exactly what it rejected before. A refused send is
// reported NotReady while the radio is not ready, and Failed otherwise.

#include "../../core/ports/MeshTransport.h"
#include "../../services/MeshService.h"
#include "../../services/MeshtasticService.h"

namespace layertime {
namespace twatch_ultra {

inline CommandResult sendResult(bool sent, bool radioReady)
{
    if (sent) return CommandResult::Ok;
    return radioReady ? CommandResult::Failed : CommandResult::NotReady;
}

inline void copyName(char (&out)[MeshNetworkStatus::kOwnNameSize], const char *name)
{
    strncpy(out, name, sizeof(out) - 1);
    out[sizeof(out) - 1] = '\0';
}

// MeshCore exposes one channel, the public group, at index 0. It has no
// direct messages on the T-Ultra.
class TUltraMeshCoreTransport : public MeshTransport {
public:
    explicit TUltraMeshCoreTransport(MeshService &service) : _service(service) {}

    MeshNetwork network() const override { return MeshNetwork::MeshCore; }

    void observeStatus(MeshNetworkStatus &out) const override { statusOf(_service.status(), out); }

    CommandResult sendText(const MeshDestination &destination, const char *text) override
    {
        if (destination.kind == MeshDestinationKind::Node) return CommandResult::Unsupported;
        if (destination.channel != 0) return CommandResult::InvalidArgument;
        const bool sent = _service.sendPublicMessage(text);
        return sendResult(sent, _service.status().radioReady);
    }

    static void statusOf(const MeshStatus &s, MeshNetworkStatus &out)
    {
        out = MeshNetworkStatus{};
        out.supported = s.supported;
        out.radioEnabled = s.radioEnabled;
        out.radioReady = s.radioReady;
        out.radioError = s.radioError;
        out.advertisingEnabled = s.advertisingEnabled;
        copyName(out.ownName, s.nodeName);
        out.nodeCount = s.nodeCount;
        // MeshStatus::messageCount counts every message ever heard. What the
        // network holds is the used slots of its six-message ring.
        uint8_t held = 0;
        for (const MeshCoreMessage &m : s.messages)
            if (m.used) ++held;
        out.messageCount = held;
    }

private:
    MeshService &_service;
};

// Meshtastic channels are its channel slots; a node is its NodeNum.
class TUltraMeshtasticTransport : public MeshTransport {
public:
    explicit TUltraMeshtasticTransport(MeshtasticService &service) : _service(service) {}

    MeshNetwork network() const override { return MeshNetwork::Meshtastic; }

    void observeStatus(MeshNetworkStatus &out) const override { statusOf(_service.status(), out); }

    CommandResult sendText(const MeshDestination &destination, const char *text) override
    {
        bool sent = false;
        if (destination.kind == MeshDestinationKind::Channel) {
            if (destination.channel >= MeshtasticStatus::kMaxChannels) return CommandResult::InvalidArgument;
            sent = _service.sendChannelMessage(destination.channel, text);
        } else {
            uint32_t nodeNum = 0;
            if (!meshtasticNodeNum(destination.node, nodeNum)) return CommandResult::InvalidArgument;
            sent = _service.sendDirectMessage(nodeNum, text);
        }
        return sendResult(sent, _service.status().radioReady);
    }

    CommandResult setChannel(uint8_t index, const char *name, const char *key) override
    {
        return _service.setChannel(index, name, key) ? CommandResult::Ok : CommandResult::InvalidArgument;
    }

    CommandResult removeChannel(uint8_t index) override
    {
        return _service.removeChannel(index) ? CommandResult::Ok : CommandResult::InvalidArgument;
    }

    static void statusOf(const MeshtasticStatus &s, MeshNetworkStatus &out)
    {
        out = MeshNetworkStatus{};
        out.supported = s.supported;
        out.radioEnabled = s.radioEnabled;
        out.radioReady = s.radioReady;
        out.radioError = s.radioError;
        out.advertisingEnabled = s.advertisingEnabled;
        copyName(out.ownName, s.longName);
        out.nodeCount = s.nodeCount;
        out.messageCount = s.messageCount;
    }

private:
    MeshtasticService &_service;
};

} // namespace twatch_ultra
} // namespace layertime
