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

// Port: one mesh network (Meshtastic or MeshCore) as the core sees it.
//
// A transport carries the core's commands out to its network and supplies
// what it observes about that network back to the core. It does not own the
// application's mesh state and it does not serve it: MeshState is the
// core's, filled from these observations.
//
// Methods returning CommandResult follow contracts/commands.md. A transport
// only checks what is specific to its network (which channels exist, what a
// node destination means there); the core has already rejected an unset
// destination, a node from another network, and empty or overlong text.

#include "../model/LayerTimeCommand.h"
#include "../model/Mesh.h"

namespace layertime {

class MeshTransport {
public:
    virtual ~MeshTransport() = default;

    virtual MeshNetwork network() const = 0;

    // The network's current status, as observed now.
    virtual void observeStatus(MeshNetworkStatus &out) const = 0;

    virtual CommandResult sendText(const MeshDestination &destination, const char *text) = 0;

    // Channel configuration. Only Meshtastic has channels to configure.
    virtual CommandResult setChannel(uint8_t, const char *, const char *) { return CommandResult::Unsupported; }
    virtual CommandResult removeChannel(uint8_t) { return CommandResult::Unsupported; }
};

} // namespace layertime
