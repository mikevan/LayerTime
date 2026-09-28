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

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(channel_conversation_holds_broadcasts_on_its_channel);
    CASE(direct_conversation_holds_both_directions_only);
    CASE(a_peer_broadcast_is_not_a_direct_message);
    CASE(direct_peer_rules);
    CASE(unread_is_strictly_after_and_never_our_own);
    CHECK_SUMMARY();
}
