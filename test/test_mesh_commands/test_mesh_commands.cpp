// Unit tests for the mesh side of src/core/app/LayerTimeCore, added in
// Phase 0 Step 5: command validation and routing (contracts/commands.md),
// quick messages by id, channel commands, MeshState filled from transport
// observations, and the conversation a removed channel leaves behind.
// Runs against fake transports; no platform code.

#include "check.h"

#include <string>
#include <vector>

#include "core/app/LayerTimeCore.h"

using namespace layertime;

namespace {

struct FakeTransport : MeshTransport {
    MeshNetwork net;
    MeshNetworkStatus observed;
    CommandResult answer = CommandResult::Ok;
    struct Sent { MeshDestination to; std::string text; };
    std::vector<Sent> sent;
    std::vector<std::string> channelCalls;

    explicit FakeTransport(MeshNetwork n) : net(n) {}
    MeshNetwork network() const override { return net; }
    void observeStatus(MeshNetworkStatus &out) const override { out = observed; }
    CommandResult sendText(const MeshDestination &d, const char *text) override
    {
        sent.push_back({d, text});
        return answer;
    }
};

struct FakeMeshtastic : FakeTransport {
    FakeMeshtastic() : FakeTransport(MeshNetwork::Meshtastic) {}
    CommandResult setChannel(uint8_t i, const char *name, const char *key) override
    {
        channelCalls.push_back("set " + std::to_string(i) + " " + name + " " + key);
        return answer;
    }
    CommandResult removeChannel(uint8_t i) override
    {
        channelCalls.push_back("remove " + std::to_string(i));
        return answer;
    }
};

struct Rig {
    FakeMeshtastic tastic;
    FakeTransport core_{MeshNetwork::MeshCore};
    LayerTimeCore core;
    Rig()
    {
        CorePorts p;
        p.mesh[static_cast<uint8_t>(MeshNetwork::Meshtastic)] = &tastic;
        p.mesh[static_cast<uint8_t>(MeshNetwork::MeshCore)] = &core_;
        core.attach(p);
    }
};

int r(CommandResult c) { return static_cast<int>(c); }

LayerTimeCommand text(MeshNetwork n, MeshDestination d, const char *t)
{
    LayerTimeCommand c;
    c.type = CommandType::MeshSendText;
    c.meshText.network = n;
    c.meshText.destination = d;
    snprintf(c.meshText.text, sizeof(c.meshText.text), "%s", t);
    return c;
}

MeshDestination channel(uint8_t i)
{
    MeshDestination d;
    d.kind = MeshDestinationKind::Channel;
    d.channel = i;
    return d;
}

MeshDestination node(MeshNodeId id)
{
    MeshDestination d;
    d.kind = MeshDestinationKind::Node;
    d.node = id;
    return d;
}

MeshNodeId corePrefix()
{
    MeshNodeId id;
    id.kind = MeshIdKind::MeshCorePublicKeyPrefix;
    id.length = 4;
    id.bytes[0] = 0xAB;
    return id;
}

} // namespace

// ---------------------------------------------------------------- MeshSendText

void text_goes_to_the_named_networks_transport()
{
    Rig g;
    CHECK_INT(r(CommandResult::Ok), r(g.core.execute(text(MeshNetwork::Meshtastic, channel(2), "hi"))));
    CHECK_INT(r(CommandResult::Ok), r(g.core.execute(text(MeshNetwork::MeshCore, channel(0), "yo"))));
    CHECK_INT(1, g.tastic.sent.size());
    CHECK_INT(1, g.core_.sent.size());
    if (g.tastic.sent.empty() || g.core_.sent.empty()) return;
    CHECK_STR("hi", g.tastic.sent[0].text.c_str());
    CHECK_INT(2, g.tastic.sent[0].to.channel);
    CHECK_STR("yo", g.core_.sent[0].text.c_str());
}

void the_transports_answer_is_the_result()
{
    Rig g;
    for (CommandResult a : {CommandResult::NotReady, CommandResult::Failed, CommandResult::InvalidArgument}) {
        g.tastic.answer = a;
        CHECK_INT(r(a), r(g.core.execute(text(MeshNetwork::Meshtastic, channel(0), "x"))));
    }
}

void an_unset_destination_is_rejected_before_the_transport()
{
    Rig g;
    LayerTimeCommand c;
    c.type = CommandType::MeshSendText;
    snprintf(c.meshText.text, sizeof(c.meshText.text), "x");
    CHECK_INT(r(CommandResult::InvalidArgument), r(g.core.execute(c)));
    CHECK_INT(0, g.tastic.sent.size());
}

void a_node_from_another_network_is_rejected()
{
    Rig g;
    CHECK_INT(r(CommandResult::InvalidArgument),
              r(g.core.execute(text(MeshNetwork::Meshtastic, node(corePrefix()), "x"))));
    CHECK_INT(r(CommandResult::InvalidArgument),
              r(g.core.execute(text(MeshNetwork::MeshCore, node(meshtasticNodeId(7)), "x"))));
    MeshNodeId bad = meshtasticNodeId(7);
    bad.length = 3;
    CHECK_INT(r(CommandResult::InvalidArgument), r(g.core.execute(text(MeshNetwork::Meshtastic, node(bad), "x"))));
    CHECK_INT(0, g.tastic.sent.size() + g.core_.sent.size());
}

void a_matching_node_goes_through()
{
    Rig g;
    CHECK_INT(r(CommandResult::Ok), r(g.core.execute(text(MeshNetwork::Meshtastic, node(meshtasticNodeId(0x1234ABCD)), "x"))));
    CHECK_INT(1, g.tastic.sent.size());
    uint32_t n = 0;
    if (!g.tastic.sent.empty()) CHECK_TRUE(meshtasticNodeNum(g.tastic.sent[0].to.node, n) && n == 0x1234ABCD);
}

void empty_text_is_rejected()
{
    Rig g;
    CHECK_INT(r(CommandResult::InvalidArgument), r(g.core.execute(text(MeshNetwork::Meshtastic, channel(0), ""))));
    CHECK_INT(0, g.tastic.sent.size());
}

void text_up_to_160_characters_goes_whole()
{
    Rig g;
    const std::string full(160, 'q');
    CHECK_INT(r(CommandResult::Ok), r(g.core.execute(text(MeshNetwork::Meshtastic, channel(0), full.c_str()))));
    if (!g.tastic.sent.empty()) CHECK_INT(160, g.tastic.sent[0].text.size());
}

void text_that_does_not_end_in_its_buffer_is_rejected()
{
    Rig g;
    LayerTimeCommand c = text(MeshNetwork::Meshtastic, channel(0), "");
    memset(c.meshText.text, 'z', sizeof(c.meshText.text));
    CHECK_INT(r(CommandResult::InvalidArgument), r(g.core.execute(c)));
    CHECK_INT(0, g.tastic.sent.size());
}

void a_network_without_a_transport_is_unsupported()
{
    LayerTimeCore bare;
    CHECK_INT(r(CommandResult::Unsupported), r(bare.execute(text(MeshNetwork::Meshtastic, channel(0), "x"))));

    // A transport in the wrong slot does not count.
    FakeTransport misplaced(MeshNetwork::MeshCore);
    LayerTimeCore wrong;
    CorePorts p;
    p.mesh[static_cast<uint8_t>(MeshNetwork::Meshtastic)] = &misplaced;
    wrong.attach(p);
    CHECK_INT(r(CommandResult::Unsupported), r(wrong.execute(text(MeshNetwork::Meshtastic, channel(0), "x"))));
    CHECK_INT(0, misplaced.sent.size());
}

// ---------------------------------------------------------------- MeshSendQuickMessage

void a_quick_message_sends_its_library_text()
{
    Rig g;
    LayerTimeCommand c;
    c.type = CommandType::MeshSendQuickMessage;
    c.meshQuick.network = MeshNetwork::Meshtastic;
    c.meshQuick.destination = node(meshtasticNodeId(42));
    c.meshQuick.quickMessageId = 11;
    CHECK_INT(r(CommandResult::Ok), r(g.core.execute(c)));
    if (!g.tastic.sent.empty()) CHECK_STR("Need help", g.tastic.sent[0].text.c_str());
}

void an_unknown_quick_message_is_rejected()
{
    Rig g;
    LayerTimeCommand c;
    c.type = CommandType::MeshSendQuickMessage;
    c.meshQuick.network = MeshNetwork::MeshCore;
    c.meshQuick.destination = channel(0);
    c.meshQuick.quickMessageId = 20;
    CHECK_INT(r(CommandResult::InvalidArgument), r(g.core.execute(c)));
    CHECK_INT(0, g.core_.sent.size());
}

void a_quick_message_is_checked_like_text()
{
    Rig g;
    LayerTimeCommand c;
    c.type = CommandType::MeshSendQuickMessage;
    c.meshQuick.network = MeshNetwork::Meshtastic;
    c.meshQuick.quickMessageId = 0;  // destination left unset
    CHECK_INT(r(CommandResult::InvalidArgument), r(g.core.execute(c)));
    LayerTimeCore bare;
    c.meshQuick.destination = channel(0);
    CHECK_INT(r(CommandResult::Unsupported), r(bare.execute(c)));
}

void the_library_is_the_twenty_default_phrases()
{
    LayerTimeCore core;
    uint8_t n = 0;
    const QuickMessage *q = core.quickMessages(n);
    CHECK_INT(20, n);
    if (n == 20) {
        CHECK_STR("Yes", q[0].text);
        CHECK_STR("Good night", q[19].text);
        for (uint8_t i = 0; i < n; ++i) CHECK_INT(i, q[i].id);
    }
}

// ---------------------------------------------------------------- channels

void channel_commands_go_to_meshtastic()
{
    Rig g;
    LayerTimeCommand c;
    c.type = CommandType::MeshSetChannel;
    c.meshChannel.index = 3;
    snprintf(c.meshChannel.name, sizeof(c.meshChannel.name), "Ops");
    snprintf(c.meshChannel.key, sizeof(c.meshChannel.key), "AQ==");
    CHECK_INT(r(CommandResult::Ok), r(g.core.execute(c)));
    c.type = CommandType::MeshRemoveChannel;
    CHECK_INT(r(CommandResult::Ok), r(g.core.execute(c)));
    CHECK_INT(2, g.tastic.channelCalls.size());
    if (g.tastic.channelCalls.size() == 2) {
        CHECK_STR("set 3 Ops AQ==", g.tastic.channelCalls[0].c_str());
        CHECK_STR("remove 3", g.tastic.channelCalls[1].c_str());
    }
    g.tastic.answer = CommandResult::InvalidArgument;
    c.type = CommandType::MeshSetChannel;
    CHECK_INT(r(CommandResult::InvalidArgument), r(g.core.execute(c)));
}

void a_48_character_key_goes_whole()
{
    Rig g;
    LayerTimeCommand c;
    c.type = CommandType::MeshSetChannel;
    c.meshChannel.index = 1;
    snprintf(c.meshChannel.name, sizeof(c.meshChannel.name), "Ops");
    const std::string key = "  " + std::string(44, 'A') + "  ";
    snprintf(c.meshChannel.key, sizeof(c.meshChannel.key), "%s", key.c_str());
    g.core.execute(c);
    if (!g.tastic.channelCalls.empty()) CHECK_STR(("set 1 Ops " + key).c_str(), g.tastic.channelCalls[0].c_str());
}

void unterminated_channel_fields_are_rejected()
{
    Rig g;
    LayerTimeCommand c;
    c.type = CommandType::MeshSetChannel;
    memset(c.meshChannel.name, 'n', sizeof(c.meshChannel.name));
    CHECK_INT(r(CommandResult::InvalidArgument), r(g.core.execute(c)));
    CHECK_INT(0, g.tastic.channelCalls.size());
}

void channel_commands_without_meshtastic_are_unsupported()
{
    FakeTransport meshcoreOnly(MeshNetwork::MeshCore);
    LayerTimeCore core;
    CorePorts p;
    p.mesh[static_cast<uint8_t>(MeshNetwork::MeshCore)] = &meshcoreOnly;
    core.attach(p);
    LayerTimeCommand c;
    c.type = CommandType::MeshSetChannel;
    CHECK_INT(r(CommandResult::Unsupported), r(core.execute(c)));
    c.type = CommandType::MeshRemoveChannel;
    CHECK_INT(r(CommandResult::Unsupported), r(core.execute(c)));
}

void removing_a_channel_drops_its_conversation_whatever_the_answer()
{
    Rig g;
    g.core.conversations().noteChannel(0);
    g.core.conversations().noteChannel(2);
    g.tastic.answer = CommandResult::InvalidArgument;
    LayerTimeCommand c;
    c.type = CommandType::MeshRemoveChannel;
    c.meshChannel.index = 2;
    g.core.execute(c);
    CHECK_TRUE(g.core.conversations().at(0).used);
    CHECK_FALSE(g.core.conversations().at(1).used);
}

// ---------------------------------------------------------------- MeshState

void mesh_state_is_filled_from_observations_on_refresh_only()
{
    Rig g;
    g.tastic.observed.supported = true;
    g.tastic.observed.radioReady = true;
    g.tastic.observed.nodeCount = 5;
    snprintf(g.tastic.observed.ownName, sizeof(g.tastic.observed.ownName), "LT-1");
    CHECK_FALSE(g.core.meshState().of(MeshNetwork::Meshtastic).supported);
    g.core.refreshMesh();
    const MeshNetworkStatus &s = g.core.meshState().of(MeshNetwork::Meshtastic);
    CHECK_TRUE(s.supported);
    CHECK_TRUE(s.radioReady);
    CHECK_INT(5, s.nodeCount);
    CHECK_STR("LT-1", s.ownName);
    // Core keeps its own copy: a later observation does not show until refreshed.
    g.tastic.observed.nodeCount = 9;
    CHECK_INT(5, g.core.meshState().of(MeshNetwork::Meshtastic).nodeCount);
}

void a_network_without_a_transport_reads_unsupported()
{
    FakeMeshtastic only;
    only.observed.supported = true;
    LayerTimeCore core;
    CorePorts p;
    p.mesh[static_cast<uint8_t>(MeshNetwork::Meshtastic)] = &only;
    core.attach(p);
    core.refreshMesh();
    CHECK_TRUE(core.meshState().of(MeshNetwork::Meshtastic).supported);
    CHECK_FALSE(core.meshState().of(MeshNetwork::MeshCore).supported);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(text_goes_to_the_named_networks_transport);
    CASE(the_transports_answer_is_the_result);
    CASE(an_unset_destination_is_rejected_before_the_transport);
    CASE(a_node_from_another_network_is_rejected);
    CASE(a_matching_node_goes_through);
    CASE(empty_text_is_rejected);
    CASE(text_up_to_160_characters_goes_whole);
    CASE(text_that_does_not_end_in_its_buffer_is_rejected);
    CASE(a_network_without_a_transport_is_unsupported);
    CASE(a_quick_message_sends_its_library_text);
    CASE(an_unknown_quick_message_is_rejected);
    CASE(a_quick_message_is_checked_like_text);
    CASE(the_library_is_the_twenty_default_phrases);
    CASE(channel_commands_go_to_meshtastic);
    CASE(a_48_character_key_goes_whole);
    CASE(unterminated_channel_fields_are_rejected);
    CASE(channel_commands_without_meshtastic_are_unsupported);
    CASE(removing_a_channel_drops_its_conversation_whatever_the_answer);
    CASE(mesh_state_is_filled_from_observations_on_refresh_only);
    CASE(a_network_without_a_transport_reads_unsupported);
    CHECK_SUMMARY();
}
