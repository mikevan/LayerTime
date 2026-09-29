// Conformance tests for the C++ binding of the LayerTime contracts.
//
// Checks that src/core/model matches contracts/vectors, that the T-Ultra
// profile matches its JSON profile, and that the new model agrees with the
// existing T-Ultra types it will be adapted from. Run from test/, see
// test/README.md.
//
// The JSON reader below is deliberately tiny. The vectors are flat objects
// written by hand, and pulling in a JSON library for them would be the first
// third-party dependency in the test harness.

#include "check.h"

#include <cstdio>
#include <cstdlib>
#include <string>

// Existing T-Ultra headers. All three are pure: no Arduino, no radio.
// Including MeshService.h alongside core/model/Mesh.h is itself a test. The
// MeshCore service's records were called MeshNode and MeshMessage, the same
// names as the core model's, until Phase 0 Step 5 renamed them MeshCoreNode
// and MeshCoreMessage; this must still compile with both in scope.
#include "services/MeshService.h"
#include "services/MeshtasticService.h"
#include "core/logic/MonitorEventLog.h"
#include "services/ReconService.h"
#include "core/logic/QuickMessages.h"

#include "core/model/Alert.h"
#include "core/model/DeviceCapabilities.h"
#include "core/model/LayerTimeCommand.h"
#include "core/model/Mesh.h"
#include "core/model/MonitorEvent.h"
#include "core/model/NavigationState.h"
#include "core/model/QuickMessage.h"
#include "core/model/Settings.h"
#include "core/model/ReconState.h"
#include "core/model/Time.h"
#include "platform/twatch_ultra/TUltraProfile.h"

// The core types are still named layertime::MeshNode and
// layertime::MeshMessage here, and the MeshCore ones ::MeshCoreNode and
// ::MeshCoreMessage, as they were written before the rename.
using namespace layertime;

namespace {

const char *kVectors = "../contracts/vectors/";

std::string readVector(const char *name)
{
    std::string path = std::string(kVectors) + name;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::printf("cannot open %s (run from test/)\n", path.c_str());
        std::exit(2);
    }
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

// A region of the text: [begin, end).
struct Span { size_t begin; size_t end; bool ok; };

Span whole(const std::string &s) { return {0, s.size(), true}; }

// Position just after `"key":` inside span, or npos.
size_t valueAt(const std::string &s, Span span, const char *key)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t p = s.find(needle, span.begin);
    while (p != std::string::npos && p < span.end) {
        size_t q = p + needle.size();
        while (q < span.end && (s[q] == ' ' || s[q] == '\n' || s[q] == '\r' || s[q] == '\t')) ++q;
        if (q < span.end && s[q] == ':') {
            ++q;
            while (q < span.end && (s[q] == ' ' || s[q] == '\n' || s[q] == '\r' || s[q] == '\t')) ++q;
            return q;
        }
        p = s.find(needle, p + 1);
    }
    return std::string::npos;
}

// The object that is the value of `key` inside span.
Span object(const std::string &s, Span span, const char *key)
{
    size_t v = valueAt(s, span, key);
    if (v == std::string::npos || s[v] != '{') return {0, 0, false};
    int depth = 0;
    bool inString = false;
    for (size_t i = v; i < span.end; ++i) {
        const char c = s[i];
        if (inString) {
            if (c == '\\') ++i;
            else if (c == '"') inString = false;
        } else if (c == '"') inString = true;
        else if (c == '{') ++depth;
        else if (c == '}' && --depth == 0) return {v + 1, i, true};
    }
    return {0, 0, false};
}

bool hasKey(const std::string &s, Span span, const char *key)
{
    return valueAt(s, span, key) != std::string::npos;
}

long long intValue(const std::string &s, Span span, const char *key)
{
    size_t v = valueAt(s, span, key);
    if (v == std::string::npos) return -1;
    return std::strtoll(s.c_str() + v, nullptr, 10);
}

std::string stringValue(const std::string &s, Span span, const char *key)
{
    size_t v = valueAt(s, span, key);
    if (v == std::string::npos || s[v] != '"') return "<missing>";
    size_t e = s.find('"', v + 1);
    return s.substr(v + 1, e - v - 1);
}

// Number of top-level keys in a flat object span.
int keyCount(const std::string &s, Span span)
{
    int n = 0;
    int depth = 0;
    bool inString = false;
    for (size_t i = span.begin; i < span.end; ++i) {
        const char c = s[i];
        if (inString) {
            if (c == '\\') ++i;
            else if (c == '"') inString = false;
        } else if (c == '"') inString = true;
        else if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') --depth;
        else if (c == ':' && depth == 0) ++n;
    }
    return n;
}

struct EnumMember { const char *name; long value; };

void checkEnum(const std::string &json, const char *enumName, const EnumMember *members, int count)
{
    Span e = object(json, whole(json), enumName);
    CHECK_TRUE(e.ok);
    if (!e.ok) return;
    CHECK_INT(count, keyCount(json, e));
    for (int i = 0; i < count; ++i) {
        const long long got = intValue(json, e, members[i].name);
        if (got != members[i].value) {
            char what[160];
            std::snprintf(what, sizeof(what), "%s.%s: vector %lld, binding %ld",
                          enumName, members[i].name, got, members[i].value);
            check::fail(__FILE__, __LINE__, what);
        }
        ++check::g_checks;
    }
}

#define M(E, name) EnumMember{#name, static_cast<long>(E::name)}

} // namespace

// ---------------------------------------------------------------- enums

void enum_values_match_vectors()
{
    const std::string j = readVector("enums.json");

    const EnumMember fix[] = {M(FixType, None), M(FixType, DeadReckoningOnly), M(FixType, Fix2D),
                              M(FixType, Fix3D)};
    checkEnum(j, "FixType", fix, 4);

    const EnumMember target[] = {
        M(ReconTarget, None), M(ReconTarget, All), M(ReconTarget, Trackers),
        M(ReconTarget, CounterSurveil), M(ReconTarget, CounterIntrusion), M(ReconTarget, Deauth),
        M(ReconTarget, Pwnagotchi), M(ReconTarget, MultiSSID), M(ReconTarget, Flock),
        M(ReconTarget, Pineapple), M(ReconTarget, AirTag), M(ReconTarget, Flipper),
        M(ReconTarget, Meta), M(ReconTarget, Axon), M(ReconTarget, Tile),
        M(ReconTarget, SamsungTag), M(ReconTarget, GoogleTag), M(ReconTarget, EarlyWarning)};
    checkEnum(j, "ReconTarget", target, 18);

    const EnumMember conf[] = {M(Confidence, Low), M(Confidence, Medium), M(Confidence, High)};
    checkEnum(j, "Confidence", conf, 3);

    const EnumMember kind[] = {M(SourceKind, Unknown), M(SourceKind, Wifi), M(SourceKind, Ble),
                               M(SourceKind, Ieee802154)};
    checkEnum(j, "SourceKind", kind, 4);

    const EnumMember band[] = {M(Band, Unknown), M(Band, Band2_4GHz), M(Band, Band5GHz)};
    checkEnum(j, "Band", band, 3);

    const EnumMember net[] = {M(MeshNetwork, Meshtastic), M(MeshNetwork, MeshCore)};
    checkEnum(j, "MeshNetwork", net, 2);

    const EnumMember idk[] = {M(MeshIdKind, None), M(MeshIdKind, MeshtasticNodeNum),
                              M(MeshIdKind, MeshCorePublicKey),
                              M(MeshIdKind, MeshCorePublicKeyPrefix)};
    checkEnum(j, "MeshIdKind", idk, 4);

    const EnumMember dk[] = {M(MeshDestinationKind, Node), M(MeshDestinationKind, Channel)};
    checkEnum(j, "MeshDestinationKind", dk, 2);

    const EnumMember del[] = {M(DeliveryState, None), M(DeliveryState, Pending),
                              M(DeliveryState, Relayed), M(DeliveryState, Acked),
                              M(DeliveryState, Failed)};
    checkEnum(j, "DeliveryState", del, 5);

    const EnumMember alert[] = {M(AlertKind, ReconDetection)};
    checkEnum(j, "AlertKind", alert, 1);

    const EnumMember cmd[] = {
        M(CommandType, None), M(CommandType, ReconStart), M(CommandType, ReconStop),
        M(CommandType, ReconClearEvents), M(CommandType, ReconAcknowledgeAlert),
        M(CommandType, MeshSendText), M(CommandType, MeshSendQuickMessage),
        M(CommandType, MeshSetChannel), M(CommandType, MeshRemoveChannel),
        M(CommandType, SetClockFormat), M(CommandType, SetUnits), M(CommandType, SetSleepMode),
        M(CommandType, SetEarlyWarning), M(CommandType, MeshSetAdvertising),
        M(CommandType, MeshSetOwnName)};
    checkEnum(j, "CommandType", cmd, 15);

    const EnumMember res[] = {M(CommandResult, Ok), M(CommandResult, Unsupported),
                              M(CommandResult, InvalidArgument), M(CommandResult, NotReady),
                              M(CommandResult, Failed)};
    checkEnum(j, "CommandResult", res, 5);

    const EnumMember dc[] = {M(DisplayClass, MonochromeLowRes), M(DisplayClass, ColorHighRes)};
    checkEnum(j, "DisplayClass", dc, 2);

    const EnumMember ds[] = {M(DisplayShape, Rectangle), M(DisplayShape, Round)};
    checkEnum(j, "DisplayShape", ds, 2);
}

void display_name_tables_cover_every_recon_target()
{
    const std::string j = readVector("enums.json");
    Span targets = object(j, whole(j), "ReconTarget");
    Span names = object(j, whole(j), "reconTargetDisplayNames");
    Span shorts = object(j, whole(j), "reconTargetShortNames");
    CHECK_TRUE(names.ok);
    CHECK_TRUE(shorts.ok);
    CHECK_INT(keyCount(j, targets), keyCount(j, names));
    CHECK_INT(keyCount(j, targets), keyCount(j, shorts));
    Span conf = object(j, whole(j), "confidenceDisplayNames");
    CHECK_INT(3, keyCount(j, conf));
}

// ---------------------------------------------------------------- sizes

void capacities_match_vectors()
{
    const std::string j = readVector("capacities.json");
    Span w = whole(j);
    CHECK_INT(MonitorEvent::kSourceIdSize, intValue(j, object(j, w, "MonitorEvent"), "sourceIdSize"));
    CHECK_INT(MonitorEvent::kDetailSize, intValue(j, object(j, w, "MonitorEvent"), "detailSize"));
    CHECK_INT(ReconState::kMaxEvents, intValue(j, object(j, w, "ReconState"), "maxEvents"));
    CHECK_INT(layertime::MeshNode::kDisplayNameSize, intValue(j, object(j, w, "MeshNode"), "displayNameSize"));
    CHECK_INT(layertime::MeshNode::kShortNameSize, intValue(j, object(j, w, "MeshNode"), "shortNameSize"));
    CHECK_INT(layertime::MeshMessage::kTextSize, intValue(j, object(j, w, "MeshMessage"), "textSize"));
    CHECK_INT(MeshNetworkStatus::kOwnNameSize,
              intValue(j, object(j, w, "MeshNetworkStatus"), "ownNameSize"));
    const Span id = object(j, w, "MeshNodeId");
    CHECK_INT(MeshNodeId::kMaxBytes, intValue(j, id, "maxBytes"));
    CHECK_INT(MeshNodeId::kMeshtasticNodeNumBytes, intValue(j, id, "meshtasticNodeNumBytes"));
    CHECK_INT(MeshNodeId::kMeshCorePublicKeyBytes, intValue(j, id, "meshCorePublicKeyBytes"));
    CHECK_INT(1, intValue(j, id, "meshCorePublicKeyPrefixMinBytes"));
    CHECK_INT(MeshNodeId::kMeshCorePublicKeyBytes - 1,
              intValue(j, id, "meshCorePublicKeyPrefixMaxBytes"));
    CHECK_INT(MeshNodeId::kMaxBytes, sizeof(MeshNodeId{}.bytes));
    CHECK_INT(QuickMessage::kTextSize, intValue(j, object(j, w, "QuickMessage"), "textSize"));
    CHECK_INT(QuickMessageLibrary::kMaxMessages,
              intValue(j, object(j, w, "QuickMessage"), "maxMessages"));
    CHECK_INT(MeshSendTextArgs::kTextSize, intValue(j, object(j, w, "MeshSendTextArgs"), "textSize"));
    CHECK_INT(ApplicationSettings::kMeshtasticNameSize,
              intValue(j, object(j, w, "ApplicationSettings"), "meshtasticNameSize"));
    CHECK_INT(SettingArgs::kNameSize, intValue(j, object(j, w, "SettingArgs"), "nameSize"));
    CHECK_INT(ApplicationSettings::kMeshtasticNameSize, sizeof(ApplicationSettings{}.meshtasticName));
    CHECK_INT(SettingArgs::kNameSize, sizeof(SettingArgs{}.name));
    CHECK_INT(MeshChannelArgs::kNameSize, intValue(j, object(j, w, "MeshChannelArgs"), "nameSize"));
    CHECK_INT(MeshChannelArgs::kKeySize, intValue(j, object(j, w, "MeshChannelArgs"), "keySize"));
    CHECK_INT(DeviceCapabilities::kProfileIdSize,
              intValue(j, object(j, w, "DeviceCapabilities"), "profileIdSize"));

    // The binding's arrays really are the declared sizes.
    CHECK_INT(MonitorEvent::kSourceIdSize, sizeof(MonitorEvent{}.sourceId));
    CHECK_INT(MonitorEvent::kDetailSize, sizeof(MonitorEvent{}.detail));
    CHECK_INT(layertime::MeshMessage::kTextSize, sizeof(layertime::MeshMessage{}.text));
    CHECK_INT(MeshSendTextArgs::kTextSize, sizeof(MeshSendTextArgs{}.text));
    CHECK_INT(MeshSendTextArgs::kMaxTextChars + 1, MeshSendTextArgs::kTextSize);
    CHECK_INT(MeshChannelArgs::kNameSize, sizeof(MeshChannelArgs{}.name));
    CHECK_INT(MeshChannelArgs::kKeySize, sizeof(MeshChannelArgs{}.key));
    CHECK_INT(QuickMessage::kTextSize, sizeof(QuickMessage{}.text));
    CHECK_INT(QuickMessageLibrary::kMaxMessages,
              sizeof(QuickMessageLibrary{}.messages) / sizeof(QuickMessage));
}

// ---------------------------------------------------------------- defaults

void defaults_claim_nothing()
{
    const Timestamp t;
    CHECK_INT(0, t.uptimeMs);
    CHECK_FALSE(t.wallClockValid);
    CHECK_INT(0, t.unixSeconds);

    const NavigationState n;
    CHECK_FALSE(n.receiverEnabled);
    CHECK_INT(static_cast<int>(FixType::None), static_cast<int>(n.fixType));
    CHECK_FALSE(n.fixUsable);
    CHECK_FALSE(n.everHadFix);
    CHECK_FALSE(n.altitudeValid);
    CHECK_FALSE(n.horizontalAccuracyValid);
    CHECK_FALSE(n.verticalAccuracyValid);
    CHECK_FALSE(n.hdopValid);
    CHECK_FALSE(n.speedValid);
    CHECK_FALSE(n.courseValid);
    CHECK_FALSE(n.headingValid);

    const layertime::MeshNode node;
    CHECK_FALSE(node.rssiValid);
    CHECK_FALSE(node.snrValid);
    CHECK_FALSE(node.batteryValid);
    CHECK_FALSE(node.positionValid);
    CHECK_FALSE(node.altitudeValid);
    CHECK_FALSE(node.positionTimeValid);
    CHECK_FALSE(node.hopsAwayValid);

    const layertime::MeshMessage msg;
    CHECK_INT(static_cast<int>(MeshIdKind::None), static_cast<int>(msg.source.kind));
    CHECK_INT(0, msg.source.length);
    // An unset destination names no one: a Node with no identity.
    CHECK_INT(static_cast<int>(MeshDestinationKind::Node), static_cast<int>(msg.destination.kind));
    CHECK_INT(static_cast<int>(MeshIdKind::None), static_cast<int>(msg.destination.node.kind));
    const LayerTimeCommand unset;
    CHECK_INT(static_cast<int>(MeshDestinationKind::Node),
              static_cast<int>(unset.meshText.destination.kind));
    CHECK_INT(static_cast<int>(MeshIdKind::None),
              static_cast<int>(unset.meshText.destination.node.kind));
    CHECK_INT(static_cast<int>(MeshIdKind::None),
              static_cast<int>(unset.meshQuick.destination.node.kind));
    CHECK_FALSE(msg.rssiValid);
    CHECK_FALSE(msg.snrValid);
    CHECK_FALSE(msg.hopsValid);
    CHECK_INT(static_cast<int>(DeliveryState::None), static_cast<int>(msg.delivery));

    const MeshNetworkStatus ns;
    CHECK_FALSE(ns.supported);
    CHECK_FALSE(ns.radioEnabled);
    CHECK_FALSE(ns.radioReady);

    const ReconState r;
    CHECK_FALSE(r.monitoring);
    CHECK_FALSE(r.alertPending);
    CHECK_INT(0, r.lastEventId);

    const LayerTimeCommand c;
    CHECK_INT(static_cast<int>(CommandType::None), static_cast<int>(c.type));
}

void capabilities_default_to_absent()
{
    const DeviceCapabilities c;
    CHECK_STR("", c.profileId);
    CHECK_INT(0, c.displayWidth);
    CHECK_INT(0, c.displayHeight);
    CHECK_FALSE(c.touch);
    CHECK_FALSE(c.buttons);
    CHECK_FALSE(c.vibration);
    CHECK_FALSE(c.gps);
    CHECK_FALSE(c.compass);
    CHECK_FALSE(c.altitude);
    CHECK_FALSE(c.nativeMaps);
    CHECK_FALSE(c.removableStorage);
    CHECK_FALSE(c.localWifiMonitor);
    CHECK_FALSE(c.localBleMonitor);
    CHECK_FALSE(c.externalRecon);
    CHECK_FALSE(c.meshUi);
    CHECK_FALSE(c.localMeshtasticRadio);
    CHECK_FALSE(c.localMeshCoreRadio);
    CHECK_FALSE(c.meshRadioShared);
    CHECK_FALSE(c.androidBridge);
}

void mesh_state_indexes_by_network()
{
    MeshState s;
    s.of(MeshNetwork::MeshCore).nodeCount = 7;
    s.of(MeshNetwork::Meshtastic).nodeCount = 3;
    CHECK_INT(3, s.networks[0].nodeCount);
    CHECK_INT(7, s.networks[1].nodeCount);
}

// ---------------------------------------------------------------- profiles
//
// A profile field carries `effective` (what DeviceCapabilities reports at
// runtime), `status`, `evidence`, and optionally `target` (design intent).
// The rule under test: effective may differ from the DeviceCapabilities
// default ONLY when status is "verified". Intent never leaks into effective.

namespace {

const char *const kCapabilityFields[] = {
    "displayClass", "displayShape", "displayWidth", "displayHeight", "touch", "buttons",
    "vibration", "gps", "compass", "altitude", "nativeMaps", "removableStorage",
    "localWifiMonitor", "localBleMonitor", "externalRecon", "meshUi",
    "localMeshtasticRadio", "localMeshCoreRadio", "meshRadioShared", "androidBridge"};
constexpr int kCapabilityFieldCount = 20;

// Effective value of one field as text, for comparing against a binding.
std::string effectiveText(const std::string &j, Span field)
{
    size_t v = valueAt(j, field, "effective");
    if (v == std::string::npos) return "<missing>";
    if (j[v] == '"') return stringValue(j, field, "effective");
    size_t e = v;
    while (e < field.end && j[e] != ',' && j[e] != ' ' && j[e] != '}') ++e;
    return j.substr(v, e - v);
}

std::string bindingText(const DeviceCapabilities &c, const char *f)
{
    const std::string n(f);
    auto b = [](bool x) { return std::string(x ? "true" : "false"); };
    if (n == "displayClass")
        return c.displayClass == DisplayClass::ColorHighRes ? "ColorHighRes" : "MonochromeLowRes";
    if (n == "displayShape") return c.displayShape == DisplayShape::Round ? "Round" : "Rectangle";
    if (n == "displayWidth") return std::to_string(c.displayWidth);
    if (n == "displayHeight") return std::to_string(c.displayHeight);
    if (n == "touch") return b(c.touch);
    if (n == "buttons") return b(c.buttons);
    if (n == "vibration") return b(c.vibration);
    if (n == "gps") return b(c.gps);
    if (n == "compass") return b(c.compass);
    if (n == "altitude") return b(c.altitude);
    if (n == "nativeMaps") return b(c.nativeMaps);
    if (n == "removableStorage") return b(c.removableStorage);
    if (n == "localWifiMonitor") return b(c.localWifiMonitor);
    if (n == "localBleMonitor") return b(c.localBleMonitor);
    if (n == "externalRecon") return b(c.externalRecon);
    if (n == "meshUi") return b(c.meshUi);
    if (n == "localMeshtasticRadio") return b(c.localMeshtasticRadio);
    if (n == "localMeshCoreRadio") return b(c.localMeshCoreRadio);
    if (n == "meshRadioShared") return b(c.meshRadioShared);
    if (n == "androidBridge") return b(c.androidBridge);
    return "<unknown field>";
}

void fieldFail(const char *profile, const char *field, const char *what,
               const std::string &a, const std::string &b)
{
    char msg[256];
    std::snprintf(msg, sizeof(msg), "%s.%s: %s (%s vs %s)", profile, field, what, a.c_str(), b.c_str());
    check::fail(__FILE__, __LINE__, msg);
}

// Structure and the effective-value rule, for any profile vector.
void checkProfileRules(const char *file)
{
    const std::string j = readVector(file);
    Span caps = object(j, whole(j), "capabilities");
    CHECK_TRUE(caps.ok);
    CHECK_INT(kCapabilityFieldCount, keyCount(j, caps));

    const DeviceCapabilities defaults;
    for (const char *f : kCapabilityFields) {
        Span field = object(j, caps, f);
        ++check::g_checks;
        if (!field.ok || !hasKey(j, field, "effective") || !hasKey(j, field, "status") ||
            !hasKey(j, field, "evidence")) {
            fieldFail(file, f, "missing effective, status, or evidence", "", "");
            continue;
        }
        const std::string status = stringValue(j, field, "status");
        ++check::g_checks;
        if (status != "verified" && status != "documented" && status != "unverified" &&
            status != "design")
            fieldFail(file, f, "unknown status", status, "");

        const std::string eff = effectiveText(j, field);
        const std::string def = bindingText(defaults, f);
        ++check::g_checks;
        if (status != "verified" && eff != def)
            fieldFail(file, f, "not verified, so effective must be the default", eff, def);
    }
}

} // namespace

void twatch_ultra_profile_obeys_effective_rule()
{
    checkProfileRules("profile_twatch_ultra.json");
}

void tactix_profile_obeys_effective_rule()
{
    checkProfileRules("profile_tactix_amoled.json");
    const std::string j = readVector("profile_tactix_amoled.json");
    CHECK_STR("tactix-amoled", stringValue(j, whole(j), "profileId").c_str());
    // Intent is recorded, not lost: every field states a target.
    Span caps = object(j, whole(j), "capabilities");
    for (const char *f : kCapabilityFields) CHECK_TRUE(hasKey(j, object(j, caps, f), "target"));
}

void twatch_ultra_binding_matches_effective_values()
{
    const std::string j = readVector("profile_twatch_ultra.json");
    const DeviceCapabilities c = twatch_ultra::capabilities();
    CHECK_STR(stringValue(j, whole(j), "profileId").c_str(), c.profileId);
    Span caps = object(j, whole(j), "capabilities");
    for (const char *f : kCapabilityFields) {
        const std::string eff = effectiveText(j, object(j, caps, f));
        const std::string bound = bindingText(c, f);
        ++check::g_checks;
        if (eff != bound) fieldFail("twatch-ultra", f, "vector effective vs binding", eff, bound);
    }
}

// ---------------------------------------------------------------- identity

namespace {
MeshNodeId makeId(MeshIdKind kind, uint8_t length, uint8_t fill)
{
    MeshNodeId id;
    id.kind = kind;
    id.length = length;
    for (uint8_t i = 0; i < length && i < MeshNodeId::kMaxBytes; ++i) id.bytes[i] = fill + i;
    return id;
}
} // namespace

void mesh_identity_lengths_follow_kind()
{
    CHECK_TRUE(meshIdWellFormed(makeId(MeshIdKind::None, 0, 0)));
    CHECK_FALSE(meshIdWellFormed(makeId(MeshIdKind::None, 4, 0)));
    CHECK_TRUE(meshIdWellFormed(makeId(MeshIdKind::MeshtasticNodeNum, 4, 0)));
    CHECK_FALSE(meshIdWellFormed(makeId(MeshIdKind::MeshtasticNodeNum, 3, 0)));
    CHECK_FALSE(meshIdWellFormed(makeId(MeshIdKind::MeshtasticNodeNum, 32, 0)));
    CHECK_TRUE(meshIdWellFormed(makeId(MeshIdKind::MeshCorePublicKey, 32, 0)));
    CHECK_FALSE(meshIdWellFormed(makeId(MeshIdKind::MeshCorePublicKey, 4, 0)));
    CHECK_TRUE(meshIdWellFormed(makeId(MeshIdKind::MeshCorePublicKeyPrefix, 1, 0)));
    CHECK_TRUE(meshIdWellFormed(makeId(MeshIdKind::MeshCorePublicKeyPrefix, 31, 0)));
    CHECK_FALSE(meshIdWellFormed(makeId(MeshIdKind::MeshCorePublicKeyPrefix, 0, 0)));
    CHECK_FALSE(meshIdWellFormed(makeId(MeshIdKind::MeshCorePublicKeyPrefix, 32, 0)));
}

void mesh_identity_network_follows_kind()
{
    MeshNetwork n = MeshNetwork::MeshCore;
    CHECK_TRUE(meshIdNetwork(MeshIdKind::MeshtasticNodeNum, n));
    CHECK_INT(static_cast<int>(MeshNetwork::Meshtastic), static_cast<int>(n));
    CHECK_TRUE(meshIdNetwork(MeshIdKind::MeshCorePublicKey, n));
    CHECK_INT(static_cast<int>(MeshNetwork::MeshCore), static_cast<int>(n));
    CHECK_TRUE(meshIdNetwork(MeshIdKind::MeshCorePublicKeyPrefix, n));
    CHECK_INT(static_cast<int>(MeshNetwork::MeshCore), static_cast<int>(n));
    CHECK_FALSE(meshIdNetwork(MeshIdKind::None, n));
}

void mesh_identity_prefix_never_matches_full_key()
{
    const MeshNodeId full = makeId(MeshIdKind::MeshCorePublicKey, 32, 7);
    const MeshNodeId sameFull = makeId(MeshIdKind::MeshCorePublicKey, 32, 7);
    const MeshNodeId otherFull = makeId(MeshIdKind::MeshCorePublicKey, 32, 8);
    // Same leading four bytes as `full`, and still not provably the same node.
    const MeshNodeId prefix = makeId(MeshIdKind::MeshCorePublicKeyPrefix, 4, 7);
    const MeshNodeId samePrefix = makeId(MeshIdKind::MeshCorePublicKeyPrefix, 4, 7);
    const MeshNodeId none = makeId(MeshIdKind::None, 0, 0);

    CHECK_TRUE(sameMeshNode(full, sameFull));
    CHECK_FALSE(sameMeshNode(full, otherFull));
    CHECK_FALSE(sameMeshNode(full, prefix));
    CHECK_FALSE(sameMeshNode(prefix, full));
    CHECK_TRUE(sameMeshNode(prefix, samePrefix));
    CHECK_FALSE(sameMeshNode(none, none));

    // Same bytes on different networks are different nodes.
    const MeshNodeId num = makeId(MeshIdKind::MeshtasticNodeNum, 4, 7);
    CHECK_FALSE(sameMeshNode(num, prefix));

    // Malformed identities are never equal, even to themselves.
    const MeshNodeId bad = makeId(MeshIdKind::MeshtasticNodeNum, 32, 7);
    CHECK_FALSE(sameMeshNode(bad, bad));
}

// ---------------------------------------------------------------- T-Ultra agreement
// The adapters in later steps are only casts and copies if these hold.

void recon_target_matches_tultra_recon_detector()
{
    static_assert(sizeof(ReconTarget) == sizeof(ReconDetector), "same width");
#define SAME(x) CHECK_INT(static_cast<int>(ReconDetector::x), static_cast<int>(ReconTarget::x))
    SAME(None); SAME(All); SAME(Trackers); SAME(CounterSurveil); SAME(CounterIntrusion);
    SAME(Deauth); SAME(Pwnagotchi); SAME(MultiSSID); SAME(Flock); SAME(Pineapple);
    SAME(AirTag); SAME(Flipper); SAME(Meta); SAME(Axon); SAME(Tile); SAME(SamsungTag);
    SAME(GoogleTag); SAME(EarlyWarning);
#undef SAME
    CHECK_INT(static_cast<int>(SignalConfidence::Low), static_cast<int>(Confidence::Low));
    CHECK_INT(static_cast<int>(SignalConfidence::Medium), static_cast<int>(Confidence::Medium));
    CHECK_INT(static_cast<int>(SignalConfidence::High), static_cast<int>(Confidence::High));
}

void monitor_event_holds_every_tultra_detection()
{
    // Phase 0 Step 4 removed the T-Ultra's own record (ReconDetection) and
    // list (ReconStatus::detections); the T-Ultra now keeps MonitorEvents in
    // core. The sizes those types had are pinned here as numbers, so the
    // model cannot shrink below what the watch stored before the move.
    CHECK_INT(40, ReconState::kMaxEvents);        // ReconStatus::MAX_DETECTIONS
    CHECK_INT(19, MonitorEvent::kSourceIdSize);   // ReconDetection::address
    CHECK_INT(40, MonitorEvent::kDetailSize);     // ReconDetection::detail
    CHECK_INT(ReconState::kMaxEvents, layertime::recon::MonitorEventLog::kCapacity);
}

void mesh_model_holds_every_tultra_mesh_record()
{
    CHECK_INT(static_cast<int>(MeshtasticDelivery::None), static_cast<int>(DeliveryState::None));
    CHECK_INT(static_cast<int>(MeshtasticDelivery::Pending), static_cast<int>(DeliveryState::Pending));
    CHECK_INT(static_cast<int>(MeshtasticDelivery::Relayed), static_cast<int>(DeliveryState::Relayed));
    CHECK_INT(static_cast<int>(MeshtasticDelivery::Acked), static_cast<int>(DeliveryState::Acked));
    CHECK_INT(static_cast<int>(MeshtasticDelivery::Failed), static_cast<int>(DeliveryState::Failed));

    // Nothing the T-Ultra stores today gets truncated by the unified model.
    CHECK_TRUE(sizeof(MeshtasticNode{}.longName) <= layertime::MeshNode::kDisplayNameSize);
    CHECK_TRUE(sizeof(::MeshCoreNode{}.name) <= layertime::MeshNode::kDisplayNameSize);
    CHECK_TRUE(sizeof(MeshtasticNode{}.shortName) <= layertime::MeshNode::kShortNameSize);
    CHECK_TRUE(sizeof(MeshtasticMessage{}.text) <= layertime::MeshMessage::kTextSize);
    CHECK_TRUE(sizeof(::MeshCoreMessage{}.text) <= layertime::MeshMessage::kTextSize);
    CHECK_TRUE(sizeof(MeshtasticStatus{}.longName) <= MeshNetworkStatus::kOwnNameSize);
    CHECK_TRUE(sizeof(MeshStatus{}.nodeName) <= MeshNetworkStatus::kOwnNameSize);
    CHECK_TRUE(sizeof(MeshtasticChannel{}.name) <= MeshChannelArgs::kNameSize);
    // A Meshtastic NodeNum is 32 bits, and fits its kind exactly.
    CHECK_INT(MeshNodeId::kMeshtasticNodeNumBytes, sizeof(MeshtasticNode{}.num));
    // The T-Ultra's MeshCore service keeps a shorter slice of the key than
    // the full key, so its adapter can only ever report a prefix. The model
    // must hold that prefix without pretending it is a full identity.
    CHECK_TRUE(sizeof(::MeshCoreNode{}.id) >= 1);
    CHECK_TRUE(sizeof(::MeshCoreNode{}.id) < MeshNodeId::kMeshCorePublicKeyBytes);
    MeshNodeId tultraMeshCore;
    tultraMeshCore.kind = MeshIdKind::MeshCorePublicKeyPrefix;
    tultraMeshCore.length = sizeof(::MeshCoreNode{}.id);
    CHECK_TRUE(meshIdWellFormed(tultraMeshCore));
    // Counts fit the uint8 fields in MeshNetworkStatus.
    CHECK_TRUE(MeshtasticStatus::kMaxNodes <= 255 && MeshtasticStatus::kMaxMessages <= 255);
}

void default_quick_messages_match_tultra_phrases()
{
    const std::string j = readVector("quick_messages_default.json");
    const size_t arr = j.find("\"messages\"");
    CHECK_TRUE(arr != std::string::npos);

    // Since Phase 0 Step 5 the T-Ultra's phrases are the core's default
    // library (src/core/logic/QuickMessages), no longer src/ui/QuickPhrases.h.
    uint8_t count = 0;
    const QuickMessage *lib = layertime::mesh::defaultQuickMessages(count);
    CHECK_INT(count, QuickMessageLibrary::kMaxMessages);

    size_t pos = arr;
    size_t joined = 0;
    for (size_t i = 0; i < count; ++i) {
        const size_t idAt = j.find("\"id\"", pos);
        CHECK_TRUE(idAt != std::string::npos);
        if (idAt == std::string::npos) return;
        Span rest{idAt, j.size(), true};
        CHECK_INT(static_cast<long>(i), intValue(j, rest, "id"));
        CHECK_INT(lib[i].id, intValue(j, rest, "id"));
        CHECK_STR(lib[i].text, stringValue(j, rest, "text").c_str());
        CHECK_TRUE(std::strlen(lib[i].text) < QuickMessage::kTextSize);
        joined += std::strlen(lib[i].text) + (i ? 1 : 0);
        pos = idAt + 1;
    }
    // No 21st entry in the vector.
    CHECK_TRUE(j.find("\"id\"", pos) == std::string::npos);
    // Meshtastic's canned-message limit.
    CHECK_TRUE(joined <= 200);
}

// Added in Phase 0 Step 5: the NodeNum encoding the contract gives for
// MeshtasticNodeNum (4 bytes, big-endian), in both directions.
void meshtastic_node_numbers_encode_big_endian()
{
    const MeshNodeId id = meshtasticNodeId(0x12345678u);
    CHECK_INT(static_cast<int>(MeshIdKind::MeshtasticNodeNum), static_cast<int>(id.kind));
    CHECK_INT(4, id.length);
    CHECK_INT(0x12, id.bytes[0]);
    CHECK_INT(0x34, id.bytes[1]);
    CHECK_INT(0x56, id.bytes[2]);
    CHECK_INT(0x78, id.bytes[3]);
    CHECK_TRUE(meshIdWellFormed(id));
    for (uint32_t n : {0u, 1u, 0x7FFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFFu, 0xDEADBEEFu}) {
        uint32_t back = 0;
        CHECK_TRUE(meshtasticNodeNum(meshtasticNodeId(n), back));
        CHECK_TRUE(back == n);
    }
    MeshNodeId other;
    other.kind = MeshIdKind::MeshCorePublicKeyPrefix;
    other.length = 4;
    uint32_t out = 7;
    CHECK_FALSE(meshtasticNodeNum(other, out));
    MeshNodeId shortId = meshtasticNodeId(5);
    shortId.length = 3;
    CHECK_FALSE(meshtasticNodeNum(shortId, out));
    CHECK_INT(7, out);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(enum_values_match_vectors);
    CASE(display_name_tables_cover_every_recon_target);
    CASE(capacities_match_vectors);
    CASE(defaults_claim_nothing);
    CASE(capabilities_default_to_absent);
    CASE(mesh_state_indexes_by_network);
    CASE(twatch_ultra_profile_obeys_effective_rule);
    CASE(tactix_profile_obeys_effective_rule);
    CASE(twatch_ultra_binding_matches_effective_values);
    CASE(mesh_identity_lengths_follow_kind);
    CASE(mesh_identity_network_follows_kind);
    CASE(mesh_identity_prefix_never_matches_full_key);
    CASE(recon_target_matches_tultra_recon_detector);
    CASE(monitor_event_holds_every_tultra_detection);
    CASE(mesh_model_holds_every_tultra_mesh_record);
    CASE(default_quick_messages_match_tultra_phrases);
    CASE(meshtastic_node_numbers_encode_big_endian);
    CHECK_SUMMARY();
}
