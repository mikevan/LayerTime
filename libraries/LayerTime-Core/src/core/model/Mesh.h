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

// Canonical definition: contracts/models.md, "Mesh".
//
// These types live in namespace layertime, as all core types do. The
// T-Ultra's MeshCore service used to declare global MeshNode and MeshMessage
// structs; Phase 0 Step 5 renamed those MeshCoreNode and MeshCoreMessage.

#include <stdint.h>
#include <string.h>

#include "Time.h"

namespace layertime {

// Numeric values are part of the contract. Do not renumber; append only.
enum class MeshNetwork : uint8_t {
    Meshtastic = 0,
    MeshCore = 1,
};

constexpr uint8_t kMeshNetworkCount = 2;

// What form a node identity is in. The kind fixes both the network and the
// meaning of the bytes, so a truncated identity can never pass for a full one.
// Numeric values are part of the contract. Do not renumber; append only.
enum class MeshIdKind : uint8_t {
    None = 0,                     // no identity: unknown or not applicable
    MeshtasticNodeNum = 1,        // 4 bytes, the NodeNum, big-endian
    MeshCorePublicKey = 2,        // 32 bytes, the full Ed25519 public key
    MeshCorePublicKeyPrefix = 3,  // 1 to 31 bytes, the leading bytes of that key
};

// A node's native identity on its own network, without truncation.
//
// The prefix kind exists because the T-Ultra's MeshCore service keeps only
// the first four bytes of each key today. Its adapter reports those as a
// prefix, which is what they are, rather than as a full identity.
struct MeshNodeId {
    static constexpr uint8_t kMeshtasticNodeNumBytes = 4;
    static constexpr uint8_t kMeshCorePublicKeyBytes = 32;
    // Largest native identity: a MeshCore public key.
    static constexpr uint8_t kMaxBytes = kMeshCorePublicKeyBytes;

    MeshIdKind kind = MeshIdKind::None;
    uint8_t length = 0;
    uint8_t bytes[kMaxBytes] = {0};
};

// The length is the one its kind requires. None is well formed only empty.
inline bool meshIdWellFormed(const MeshNodeId &id)
{
    switch (id.kind) {
    case MeshIdKind::None: return id.length == 0;
    case MeshIdKind::MeshtasticNodeNum: return id.length == MeshNodeId::kMeshtasticNodeNumBytes;
    case MeshIdKind::MeshCorePublicKey: return id.length == MeshNodeId::kMeshCorePublicKeyBytes;
    case MeshIdKind::MeshCorePublicKeyPrefix:
        return id.length >= 1 && id.length < MeshNodeId::kMeshCorePublicKeyBytes;
    }
    return false;
}

// The network a kind of identity belongs to. False for None.
inline bool meshIdNetwork(MeshIdKind kind, MeshNetwork &out)
{
    switch (kind) {
    case MeshIdKind::MeshtasticNodeNum: out = MeshNetwork::Meshtastic; return true;
    case MeshIdKind::MeshCorePublicKey:
    case MeshIdKind::MeshCorePublicKeyPrefix: out = MeshNetwork::MeshCore; return true;
    default: return false;
    }
}

// True only when both identities are present, of the same kind and length,
// and byte-for-byte equal. A prefix is never the same node as a full key,
// because a prefix cannot prove it: two keys can share one.
inline bool sameMeshNode(const MeshNodeId &a, const MeshNodeId &b)
{
    return a.kind != MeshIdKind::None && a.kind == b.kind && a.length == b.length &&
           meshIdWellFormed(a) && memcmp(a.bytes, b.bytes, a.length) == 0;
}

// A Meshtastic NodeNum as an identity: 4 bytes, big-endian.
inline MeshNodeId meshtasticNodeId(uint32_t nodeNum)
{
    MeshNodeId id;
    id.kind = MeshIdKind::MeshtasticNodeNum;
    id.length = MeshNodeId::kMeshtasticNodeNumBytes;
    id.bytes[0] = static_cast<uint8_t>(nodeNum >> 24);
    id.bytes[1] = static_cast<uint8_t>(nodeNum >> 16);
    id.bytes[2] = static_cast<uint8_t>(nodeNum >> 8);
    id.bytes[3] = static_cast<uint8_t>(nodeNum);
    return id;
}

// The NodeNum back out of an identity. False unless it is a well-formed
// Meshtastic NodeNum.
inline bool meshtasticNodeNum(const MeshNodeId &id, uint32_t &out)
{
    if (id.kind != MeshIdKind::MeshtasticNodeNum || !meshIdWellFormed(id)) return false;
    out = (static_cast<uint32_t>(id.bytes[0]) << 24) | (static_cast<uint32_t>(id.bytes[1]) << 16) |
          (static_cast<uint32_t>(id.bytes[2]) << 8) | static_cast<uint32_t>(id.bytes[3]);
    return true;
}

// Where a message goes, stated as what it means rather than as a special
// node number. The network is carried alongside, by the message or command.
enum class MeshDestinationKind : uint8_t {
    Node = 0,     // one node, named by `node`
    Channel = 1,  // everyone on channel `channel`
};

// The default is a Node destination with no identity, which is not a valid
// target. A command built without setting its destination is rejected as
// InvalidArgument instead of going out on a channel by accident.
struct MeshDestination {
    MeshDestinationKind kind = MeshDestinationKind::Node;
    // Meaningful only when kind is Node.
    MeshNodeId node;
    // Meaningful only when kind is Channel. An index into that network's
    // channel list as the platform exposes it.
    uint8_t channel = 0;
};

enum class DeliveryState : uint8_t {
    None = 0,     // received message, or sent with no delivery tracking
    Pending = 1,  // sent, nothing heard back yet
    Relayed = 2,  // heard a neighbour rebroadcast it
    Acked = 3,    // destination acknowledged it
    Failed = 4,   // retries exhausted
};

// A node heard on either mesh. Protocol-specific detail (telemetry voltage,
// channel utilisation, public keys, MeshCore node type) stays in the
// network's own service until a UI shows a demonstrated need for it here.
struct MeshNode {
    static constexpr uint8_t kDisplayNameSize = 40;
    static constexpr uint8_t kShortNameSize = 8;

    // The node's network is the network of its id kind (meshIdNetwork).
    MeshNodeId id;
    char displayName[kDisplayNameSize] = {0};
    // Meshtastic short name. Empty on MeshCore.
    char shortName[kShortNameSize] = {0};

    Timestamp lastSeen;

    bool rssiValid = false;
    float rssi = 0.0f;
    bool snrValid = false;
    float snr = 0.0f;

    bool batteryValid = false;
    uint8_t batteryPercent = 0;  // 0 to 100
    bool externalPower = false;  // Meshtastic reports >100 for this

    bool positionValid = false;
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    bool altitudeValid = false;
    int32_t altitudeM = 0;
    // When the position itself was reported. Not the same as lastSeen: a node
    // heard a minute ago may be carrying a position from an hour ago.
    bool positionTimeValid = false;
    Timestamp positionTime;

    bool hopsAwayValid = false;
    uint8_t hopsAway = 0;
};

struct MeshMessage {
    static constexpr uint8_t kTextSize = 160;

    MeshNetwork network = MeshNetwork::Meshtastic;

    // Kind None when the sender is unknown. MeshCore public group text
    // carries the sender only inside the text itself.
    MeshNodeId source;
    MeshDestination destination;

    bool fromSelf = false;
    DeliveryState delivery = DeliveryState::None;

    char text[kTextSize] = {0};

    Timestamp received;

    bool rssiValid = false;
    float rssi = 0.0f;
    bool snrValid = false;
    float snr = 0.0f;
    bool hopsValid = false;
    uint8_t hops = 0;
};

// The state of one mesh network's radio and service.
struct MeshNetworkStatus {
    static constexpr uint8_t kOwnNameSize = 24;

    // This platform has this network at all.
    bool supported = false;
    bool radioEnabled = false;
    bool radioReady = false;
    // Platform radio error code when enabled but not ready. 0 otherwise.
    int16_t radioError = 0;
    bool advertisingEnabled = false;
    char ownName[kOwnNameSize] = {0};

    uint8_t nodeCount = 0;
    uint8_t messageCount = 0;
};

// Both networks. Application state, owned by the core and filled from what
// each network's transport observes. The shared node and message lists are
// not built yet: in Phase 0 the T-Ultra's own screens still render from each
// network's service, and nothing else reads them.
struct MeshState {
    MeshNetworkStatus networks[kMeshNetworkCount];

    const MeshNetworkStatus &of(MeshNetwork network) const
    {
        return networks[static_cast<uint8_t>(network)];
    }
    MeshNetworkStatus &of(MeshNetwork network)
    {
        return networks[static_cast<uint8_t>(network)];
    }
};

} // namespace layertime
