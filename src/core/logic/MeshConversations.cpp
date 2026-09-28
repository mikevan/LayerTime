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

#include "MeshConversations.h"

namespace layertime {
namespace mesh {

bool messageInConversation(const MessageFacts &m, const ConversationKey &c, uint32_t us)
{
    if (c.isChannel) return m.toBroadcast && m.channel == c.channel;
    if (m.fromSelf) return m.toNum == c.peer;
    return m.fromNum == c.peer && m.toNum == us;
}

bool directPeerFor(const MessageFacts &m, uint32_t us, uint32_t &peer)
{
    if (m.toBroadcast) return false;
    if (m.fromSelf) {
        peer = m.toNum;
        return true;
    }
    if (m.toNum == us) {
        peer = m.fromNum;
        return true;
    }
    return false;
}

bool isUnread(bool fromSelf, uint32_t receivedMs, uint32_t lastViewedMs)
{
    return !fromSelf && receivedMs > lastViewedMs;
}

} // namespace mesh
} // namespace layertime
