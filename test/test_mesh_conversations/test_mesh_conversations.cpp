// Unit tests for src/core/logic/MeshConversations, moved out of
// MeshtasticScreen in Phase 0 Step 3h. test_meshtastic_chats still checks
// the screen; this suite checks the rules directly, with a non-zero node
// number for "us", which the screen test cannot set.

#include "check.h"

#include "core/logic/MeshConversations.h"

using namespace layertime::mesh;

namespace {
constexpr uint32_t kUs = 0x0A0B0C0D;
constexpr uint32_t kPeer = 0x11112222;
constexpr uint32_t kOther = 0x33334444;

MessageFacts msg(bool self, bool bcast, uint32_t from, uint32_t to, uint8_t ch)
{
    MessageFacts m;
    m.fromSelf = self;
    m.toBroadcast = bcast;
    m.fromNum = from;
    m.toNum = to;
    m.channel = ch;
    return m;
}
ConversationKey channel(uint8_t ch) { ConversationKey k; k.isChannel = true; k.channel = ch; return k; }
ConversationKey direct(uint32_t peer) { ConversationKey k; k.peer = peer; return k; }
} // namespace

void channel_conversation_holds_broadcasts_on_its_channel()
{
    CHECK_TRUE(messageInConversation(msg(false, true, kPeer, 0xFFFFFFFF, 2), channel(2), kUs));
    CHECK_TRUE(messageInConversation(msg(true, true, kUs, 0xFFFFFFFF, 2), channel(2), kUs));
    CHECK_FALSE(messageInConversation(msg(false, true, kPeer, 0xFFFFFFFF, 1), channel(2), kUs));
    CHECK_FALSE(messageInConversation(msg(false, false, kPeer, kUs, 2), channel(2), kUs));
}

void direct_conversation_holds_both_directions_only()
{
    CHECK_TRUE(messageInConversation(msg(false, false, kPeer, kUs, 0), direct(kPeer), kUs));
    CHECK_TRUE(messageInConversation(msg(true, false, kUs, kPeer, 0), direct(kPeer), kUs));
    CHECK_FALSE(messageInConversation(msg(false, false, kPeer, kOther, 0), direct(kPeer), kUs));
    CHECK_FALSE(messageInConversation(msg(false, false, kOther, kUs, 0), direct(kPeer), kUs));
    CHECK_FALSE(messageInConversation(msg(true, false, kUs, kOther, 0), direct(kPeer), kUs));
    // The channel a direct message arrived on does not matter.
    CHECK_TRUE(messageInConversation(msg(false, false, kPeer, kUs, 5), direct(kPeer), kUs));
}

void a_peer_broadcast_is_not_a_direct_message()
{
    CHECK_FALSE(messageInConversation(msg(false, true, kPeer, 0xFFFFFFFF, 0), direct(kPeer), kUs));
}

void direct_peer_rules()
{
    uint32_t peer = 0;
    CHECK_TRUE(directPeerFor(msg(true, false, kUs, kPeer, 0), kUs, peer));
    CHECK_INT(kPeer, peer);
    peer = 0;
    CHECK_TRUE(directPeerFor(msg(false, false, kPeer, kUs, 0), kUs, peer));
    CHECK_INT(kPeer, peer);
    peer = 7;
    CHECK_FALSE(directPeerFor(msg(false, false, kPeer, kOther, 0), kUs, peer));
    CHECK_FALSE(directPeerFor(msg(false, true, kPeer, 0xFFFFFFFF, 0), kUs, peer));
    CHECK_FALSE(directPeerFor(msg(true, true, kUs, 0xFFFFFFFF, 0), kUs, peer));
    CHECK_INT(7, peer);  // untouched when there is no peer
}

void unread_is_strictly_after_and_never_our_own()
{
    CHECK_TRUE(isUnread(false, 1001, 1000));
    CHECK_FALSE(isUnread(false, 1000, 1000));
    CHECK_FALSE(isUnread(false, 999, 1000));
    CHECK_FALSE(isUnread(true, 5000, 1000));
    CHECK_TRUE(isUnread(false, 1, 0));
}

// ---------------------------------------------------------------- the table
// ConversationTable moved from MeshtasticScreen into core in Phase 0 Step 5.
// Its behaviour through the screen is pinned by test_meshtastic_chats; these
// check it on its own.

void the_table_holds_24_in_first_free_slot_order()
{
    ConversationTable t;
    CHECK_INT(24, ConversationTable::kCapacity);
    for (uint8_t i = 0; i < 8; ++i) t.noteChannel(i);
    for (uint32_t p = 1; p <= 20; ++p) t.noteMessage(msg(false, false, p, kUs, 0), kUs);
    uint8_t used = 0;
    for (uint8_t i = 0; i < ConversationTable::kCapacity; ++i) used += t.at(i).used ? 1 : 0;
    CHECK_INT(24, used);
    CHECK_TRUE(t.at(0).key.isChannel && t.at(0).key.channel == 0);
    CHECK_TRUE(!t.at(8).key.isChannel && t.at(8).key.peer == 1);
    CHECK_TRUE(t.at(23).key.peer == 16);  // peers 17 to 20 did not fit
    CHECK_TRUE(t.findOrAdd(direct(99)) == nullptr);
}

void a_direct_conversation_ignores_its_channel()
{
    ConversationTable t;
    ConversationKey a = direct(kPeer);
    a.channel = 5;
    ConversationTable::Entry *e1 = t.findOrAdd(a);
    ConversationTable::Entry *e2 = t.findOrAdd(direct(kPeer));
    CHECK_TRUE(e1 != nullptr && e1 == e2);
    if (e1) CHECK_INT(0, e1->key.channel);
}

void a_channel_conversation_is_one_per_slot()
{
    ConversationTable t;
    t.noteChannel(2);
    t.noteChannel(2);
    ConversationKey k = channel(2);
    k.peer = 0xFFFFFFFF;
    CHECK_TRUE(t.findOrAdd(k) == &t.at(0));
    CHECK_FALSE(t.at(1).used);
}

void only_traffic_with_us_makes_a_direct_conversation()
{
    ConversationTable t;
    t.noteMessage(msg(false, true, kPeer, 0xFFFFFFFF, 0), kUs);  // broadcast
    t.noteMessage(msg(false, false, kPeer, kOther, 0), kUs);      // between others
    CHECK_FALSE(t.at(0).used);
    t.noteMessage(msg(true, false, kUs, kPeer, 0), kUs);          // we sent it
    CHECK_TRUE(t.at(0).used && t.at(0).key.peer == kPeer);
}

void mark_viewed_adds_if_needed_and_sets_the_time()
{
    ConversationTable t;
    t.markViewed(direct(kPeer), 5000);
    CHECK_TRUE(t.at(0).used);
    CHECK_INT(5000, t.at(0).lastViewedMs);
    t.markViewed(direct(kPeer), 7000);
    CHECK_INT(7000, t.at(0).lastViewedMs);
    CHECK_FALSE(t.at(1).used);
}

void removing_a_channel_frees_its_slot_for_reuse()
{
    ConversationTable t;
    t.noteChannel(0);
    t.noteChannel(3);
    t.markViewed(channel(3), 900);
    t.removeChannel(3);
    CHECK_FALSE(t.at(1).used);
    t.noteMessage(msg(false, false, kPeer, kUs, 0), kUs);
    CHECK_TRUE(t.at(1).used && t.at(1).key.peer == kPeer);
    CHECK_INT(0, t.at(1).lastViewedMs);
    t.noteChannel(3);  // comes back unread-from-zero in the next free slot
    CHECK_TRUE(t.at(2).used && t.at(2).key.channel == 3 && t.at(2).lastViewedMs == 0);
}

void counts_is_unread_and_in_this_conversation()
{
    ConversationTable t;
    t.markViewed(direct(kPeer), 1000);
    const ConversationTable::Entry &e = t.at(0);
    CHECK_TRUE(ConversationTable::counts(e, msg(false, false, kPeer, kUs, 0), 1001, kUs));
    CHECK_FALSE(ConversationTable::counts(e, msg(false, false, kPeer, kUs, 0), 1000, kUs));
    CHECK_FALSE(ConversationTable::counts(e, msg(true, false, kUs, kPeer, 0), 5000, kUs));
    CHECK_FALSE(ConversationTable::counts(e, msg(false, false, kOther, kUs, 0), 5000, kUs));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(channel_conversation_holds_broadcasts_on_its_channel);
    CASE(direct_conversation_holds_both_directions_only);
    CASE(a_peer_broadcast_is_not_a_direct_message);
    CASE(direct_peer_rules);
    CASE(unread_is_strictly_after_and_never_our_own);
    CASE(the_table_holds_24_in_first_free_slot_order);
    CASE(a_direct_conversation_ignores_its_channel);
    CASE(a_channel_conversation_is_one_per_slot);
    CASE(only_traffic_with_us_makes_a_direct_conversation);
    CASE(mark_viewed_adds_if_needed_and_sets_the_time);
    CASE(removing_a_channel_frees_its_slot_for_reuse);
    CASE(counts_is_unread_and_in_this_conversation);
    CHECK_SUMMARY();
}
