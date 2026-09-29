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

namespace {
ConversationKey normalized(const ConversationKey &key)
{
    ConversationKey k = key;
    if (k.isChannel) k.peer = 0;
    else k.channel = 0;
    return k;
}

bool sameKey(const ConversationKey &a, const ConversationKey &b)
{
    return a.isChannel == b.isChannel && a.peer == b.peer && a.channel == b.channel;
}
} // namespace

ConversationTable::Entry *ConversationTable::findOrAdd(const ConversationKey &key)
{
    const ConversationKey k = normalized(key);
    for (Entry &e : _entries)
        if (e.used && sameKey(e.key, k)) return &e;
    for (Entry &e : _entries) {
        if (!e.used) {
            e.used = true;
            e.key = k;
            e.lastViewedMs = 0;
            return &e;
        }
    }
    return nullptr;
}

void ConversationTable::noteChannel(uint8_t channel)
{
    ConversationKey k;
    k.isChannel = true;
    k.channel = channel;
    findOrAdd(k);
}

void ConversationTable::noteMessage(const MessageFacts &message, uint32_t ourNodeNum)
{
    uint32_t peer = 0;
    if (!directPeerFor(message, ourNodeNum, peer)) return;
    ConversationKey k;
    k.peer = peer;
    findOrAdd(k);
}

void ConversationTable::markViewed(const ConversationKey &key, uint32_t nowMs)
{
    Entry *e = findOrAdd(key);
    if (e) e->lastViewedMs = nowMs;
}

void ConversationTable::removeChannel(uint8_t channel)
{
    for (Entry &e : _entries)
        if (e.used && e.key.isChannel && e.key.channel == channel) e.used = false;
}

bool ConversationTable::counts(const Entry &entry, const MessageFacts &message, uint32_t receivedMs,
                               uint32_t ourNodeNum)
{
    return isUnread(message.fromSelf, receivedMs, entry.lastViewedMs) &&
           messageInConversation(message, entry.key, ourNodeNum);
}

} // namespace mesh
} // namespace layertime
