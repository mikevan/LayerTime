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

// Which Meshtastic messages belong to which conversation, which messages
// start a direct conversation, and what counts as unread. Moved unchanged
// out of MeshtasticScreen in Phase 0 Step 3h. The conversation table itself
// (its 24 slots and read times) followed in Step 5, also unchanged.
//
// Messages are described by plain facts rather than the service's struct.
// "Broadcast" is a fact about the message, not a special node number.

#include <stdint.h>

namespace layertime {
namespace mesh {

struct MessageFacts {
    bool fromSelf = false;     // we sent it
    bool toBroadcast = false;  // addressed to a channel, not a node
    uint32_t fromNum = 0;
    uint32_t toNum = 0;        // destination node number as received
    uint8_t channel = 0;       // meaningful only when toBroadcast is true
};

// A conversation is either a channel (everyone on channel `channel`) or a
// direct exchange with node `peer`.
struct ConversationKey {
    bool isChannel = false;
    uint32_t peer = 0;
    uint8_t channel = 0;
};

// Channel conversation: broadcast messages on that channel. Direct
// conversation: what we sent to the peer, plus what the peer sent to us.
bool messageInConversation(const MessageFacts &message, const ConversationKey &conversation,
                           uint32_t ourNodeNum);

// The node a message opens a direct conversation with, if any. Broadcasts
// open none, as does traffic between two other nodes.
bool directPeerFor(const MessageFacts &message, uint32_t ourNodeNum, uint32_t &peer);

// Unread: received from someone else strictly after the conversation was
// last viewed. Our own messages are never unread.
bool isUnread(bool fromSelf, uint32_t receivedMs, uint32_t lastViewedMs);

// The conversations the wearer has, and when each was last opened. Owned by
// the core. Keyed by Meshtastic node number: the only network with direct
// conversations today.
//
// The messages themselves are not stored here. Whoever renders a
// conversation passes its messages in (as MessageFacts) to decide which
// conversations exist and what is unread.
class ConversationTable {
public:
    // Every Meshtastic channel slot, plus 16 direct peers.
    static constexpr uint8_t kCapacity = 24;

    struct Entry {
        bool used = false;
        ConversationKey key;
        // When the wearer last opened it. 0 until then.
        uint32_t lastViewedMs = 0;
    };

    // The conversation, added in the first free slot if it is new. A
    // direct conversation is per node, whichever channel carried it, so its
    // channel is always 0. nullptr when the table is full.
    Entry *findOrAdd(const ConversationKey &key);

    // Every configured channel is a conversation, even with no messages.
    void noteChannel(uint8_t channel);
    // A direct conversation exists once any message passes either way.
    void noteMessage(const MessageFacts &message, uint32_t ourNodeNum);

    // Opening a conversation marks everything received so far as read.
    void markViewed(const ConversationKey &key, uint32_t nowMs);

    // A deleted channel stops being a conversation.
    void removeChannel(uint8_t channel);

    // Slot order, which is the order conversations were first seen.
    const Entry &at(uint8_t slot) const { return _entries[slot]; }

    // Whether one message counts toward this conversation's unread total.
    static bool counts(const Entry &entry, const MessageFacts &message, uint32_t receivedMs,
                       uint32_t ourNodeNum);

private:
    Entry _entries[kCapacity];
};

} // namespace mesh
} // namespace layertime
