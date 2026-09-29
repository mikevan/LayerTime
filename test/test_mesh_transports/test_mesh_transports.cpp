// Unit tests for the T-Ultra mesh transports
// (src/platform/twatch_ultra/TUltraMeshTransports.h), added in Phase 0
// Step 5: what each service's status becomes in MeshNetworkStatus, which
// service call a destination turns into, and how a refused send is
// reported. The two services' send and channel methods are test-only fakes
// below that record each call and answer what the test sets.

#include "check.h"

#include <memory>
#include <string>
#include <vector>

#include "platform/twatch_ultra/TUltraMeshTransports.h"

using namespace layertime;
using namespace layertime::twatch_ultra;

// ---------------------------------------------------------------- link seams (test only)

namespace fake {
inline std::vector<std::string> g_calls;
inline bool g_answer = true;
}

bool MeshService::sendPublicMessage(const char *t)
{
    fake::g_calls.push_back(std::string("public ") + t);
    return fake::g_answer;
}
bool MeshtasticService::sendChannelMessage(uint8_t i, const char *t)
{
    fake::g_calls.push_back("channel " + std::to_string(i) + " " + t);
    return fake::g_answer;
}
bool MeshtasticService::sendDirectMessage(uint32_t n, const char *t)
{
    fake::g_calls.push_back("direct " + std::to_string(n) + " " + t);
    return fake::g_answer;
}
bool MeshtasticService::setChannel(uint8_t i, const char *name, const char *key)
{
    fake::g_calls.push_back("set " + std::to_string(i) + " " + name + " " + key);
    return fake::g_answer;
}
bool MeshtasticService::removeChannel(uint8_t i)
{
    fake::g_calls.push_back("remove " + std::to_string(i));
    return fake::g_answer;
}

namespace {

int r(CommandResult c) { return static_cast<int>(c); }

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

void reset(bool answer = true)
{
    fake::g_calls.clear();
    fake::g_answer = answer;
}

} // namespace

// ---------------------------------------------------------------- results

void a_refused_send_is_not_ready_until_the_radio_is_up()
{
    CHECK_INT(r(CommandResult::Ok), r(sendResult(true, false)));
    CHECK_INT(r(CommandResult::Ok), r(sendResult(true, true)));
    CHECK_INT(r(CommandResult::NotReady), r(sendResult(false, false)));
    CHECK_INT(r(CommandResult::Failed), r(sendResult(false, true)));
}

// ---------------------------------------------------------------- MeshCore

void meshcore_channel_0_is_its_public_group()
{
    reset();
    MeshService service;
    TUltraMeshCoreTransport t(service);
    CHECK_INT(static_cast<int>(MeshNetwork::MeshCore), static_cast<int>(t.network()));
    CHECK_INT(r(CommandResult::Ok), r(t.sendText(channel(0), "hello")));
    CHECK_INT(1, fake::g_calls.size());
    if (!fake::g_calls.empty()) CHECK_STR("public hello", fake::g_calls[0].c_str());
}

void meshcore_has_no_other_channel_and_no_direct_messages()
{
    reset();
    MeshService service;
    TUltraMeshCoreTransport t(service);
    CHECK_INT(r(CommandResult::InvalidArgument), r(t.sendText(channel(1), "x")));
    MeshNodeId prefix;
    prefix.kind = MeshIdKind::MeshCorePublicKeyPrefix;
    prefix.length = 4;
    CHECK_INT(r(CommandResult::Unsupported), r(t.sendText(node(prefix), "x")));
    CHECK_INT(r(CommandResult::Unsupported), r(t.setChannel(1, "a", "0")));
    CHECK_INT(r(CommandResult::Unsupported), r(t.removeChannel(1)));
    CHECK_INT(0, fake::g_calls.size());
}

void meshcore_refusal_with_the_radio_down_is_not_ready()
{
    reset(false);
    MeshService service;  // radio not ready until it is powered up
    TUltraMeshCoreTransport t(service);
    CHECK_INT(r(CommandResult::NotReady), r(t.sendText(channel(0), "x")));
}

void meshcore_status_counts_messages_held_not_messages_ever()
{
    std::unique_ptr<MeshStatus> s(new MeshStatus());
    s->supported = true;
    s->radioEnabled = true;
    s->radioReady = true;
    s->radioError = 0;
    s->advertisingEnabled = true;
    snprintf(s->nodeName, sizeof(s->nodeName), "%s", "LT-ab12");
    s->nodeCount = 3;
    s->messageCount = 57;  // every message heard since boot
    s->messages[0].used = true;
    s->messages[4].used = true;
    MeshNetworkStatus out;
    out.messageCount = 99;
    TUltraMeshCoreTransport::statusOf(*s, out);
    CHECK_TRUE(out.supported && out.radioEnabled && out.radioReady && out.advertisingEnabled);
    CHECK_STR("LT-ab12", out.ownName);
    CHECK_INT(3, out.nodeCount);
    CHECK_INT(2, out.messageCount);
}

void meshcore_error_code_is_carried()
{
    std::unique_ptr<MeshStatus> s(new MeshStatus());
    s->supported = true;
    s->radioEnabled = true;
    s->radioError = -707;
    MeshNetworkStatus out;
    TUltraMeshCoreTransport::statusOf(*s, out);
    CHECK_FALSE(out.radioReady);
    CHECK_INT(-707, out.radioError);
}

// ---------------------------------------------------------------- Meshtastic

void meshtastic_channels_are_its_slots()
{
    reset();
    MeshtasticService service;
    TUltraMeshtasticTransport t(service);
    CHECK_INT(static_cast<int>(MeshNetwork::Meshtastic), static_cast<int>(t.network()));
    CHECK_INT(r(CommandResult::Ok), r(t.sendText(channel(3), "hi")));
    CHECK_INT(r(CommandResult::InvalidArgument), r(t.sendText(channel(MeshtasticStatus::kMaxChannels), "hi")));
    CHECK_INT(1, fake::g_calls.size());
    if (!fake::g_calls.empty()) CHECK_STR("channel 3 hi", fake::g_calls[0].c_str());
}

void meshtastic_nodes_are_their_node_numbers()
{
    reset();
    MeshtasticService service;
    TUltraMeshtasticTransport t(service);
    CHECK_INT(r(CommandResult::Ok), r(t.sendText(node(meshtasticNodeId(0xDEADBEEF)), "dm")));
    CHECK_INT(1, fake::g_calls.size());
    if (!fake::g_calls.empty()) CHECK_STR("direct 3735928559 dm", fake::g_calls[0].c_str());
    MeshNodeId wrong;
    wrong.kind = MeshIdKind::MeshCorePublicKeyPrefix;
    wrong.length = 4;
    CHECK_INT(r(CommandResult::InvalidArgument), r(t.sendText(node(wrong), "dm")));
    CHECK_INT(1, fake::g_calls.size());
}

void meshtastic_channel_configuration_is_passed_through()
{
    reset();
    MeshtasticService service;
    TUltraMeshtasticTransport t(service);
    CHECK_INT(r(CommandResult::Ok), r(t.setChannel(0, "x", "1")));  // the service decides
    CHECK_INT(r(CommandResult::Ok), r(t.removeChannel(0)));
    fake::g_answer = false;
    CHECK_INT(r(CommandResult::InvalidArgument), r(t.setChannel(2, "", "zz")));
    CHECK_INT(r(CommandResult::InvalidArgument), r(t.removeChannel(9)));
    CHECK_INT(4, fake::g_calls.size());
    if (fake::g_calls.size() == 4) {
        CHECK_STR("set 0 x 1", fake::g_calls[0].c_str());
        CHECK_STR("remove 0", fake::g_calls[1].c_str());
        CHECK_STR("set 2  zz", fake::g_calls[2].c_str());
        CHECK_STR("remove 9", fake::g_calls[3].c_str());
    }
}

void meshtastic_refusal_with_the_radio_down_is_not_ready()
{
    reset(false);
    MeshtasticService service;
    TUltraMeshtasticTransport t(service);
    CHECK_INT(r(CommandResult::NotReady), r(t.sendText(channel(0), "x")));
}

void meshtastic_status()
{
    std::unique_ptr<MeshtasticStatus> s(new MeshtasticStatus());
    s->supported = true;
    s->radioEnabled = true;
    s->radioReady = true;
    s->advertisingEnabled = false;
    snprintf(s->longName, sizeof(s->longName), "%s", "Ranger 7");
    s->nodeCount = 64;
    s->messageCount = 96;
    MeshNetworkStatus out;
    TUltraMeshtasticTransport::statusOf(*s, out);
    CHECK_TRUE(out.supported && out.radioEnabled && out.radioReady);
    CHECK_FALSE(out.advertisingEnabled);
    CHECK_STR("Ranger 7", out.ownName);
    CHECK_INT(64, out.nodeCount);
    CHECK_INT(96, out.messageCount);
}

void own_names_are_bounded_by_the_model()
{
    std::unique_ptr<MeshtasticStatus> s(new MeshtasticStatus());
    memset(s->longName, 'n', sizeof(s->longName) - 1);
    MeshNetworkStatus out;
    TUltraMeshtasticTransport::statusOf(*s, out);
    CHECK_INT(MeshNetworkStatus::kOwnNameSize - 1, std::strlen(out.ownName));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(a_refused_send_is_not_ready_until_the_radio_is_up);
    CASE(meshcore_channel_0_is_its_public_group);
    CASE(meshcore_has_no_other_channel_and_no_direct_messages);
    CASE(meshcore_refusal_with_the_radio_down_is_not_ready);
    CASE(meshcore_status_counts_messages_held_not_messages_ever);
    CASE(meshcore_error_code_is_carried);
    CASE(meshtastic_channels_are_its_slots);
    CASE(meshtastic_nodes_are_their_node_numbers);
    CASE(meshtastic_channel_configuration_is_passed_through);
    CASE(meshtastic_refusal_with_the_radio_down_is_not_ready);
    CASE(meshtastic_status);
    CASE(own_names_are_bounded_by_the_model);
    CHECK_SUMMARY();
}
