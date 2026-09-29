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

#include <stdint.h>
#include <stddef.h>

// Passive (receive-only) Meshtastic listener for the US/LongFast default
// public channel. Built as a parallel structure to MeshService (MeshCore) -
// deliberately not sharing code with it, so the two can be compared. Only
// one of the two can own the physical SX1262 radio at a time; WatchApp is
// responsible for keeping their "enabled" settings mutually exclusive
// before calling setRadioEnabled() on either.
struct MeshtasticNode {
    bool used = false;
    uint32_t num = 0; // Meshtastic NodeNum, from the packet header's "from" field.
    char longName[40] = {0};
    char shortName[8] = {0};

    bool hasPosition = false;
    double latitude = 0.0;
    double longitude = 0.0;
    int32_t altitudeM = 0;

    bool hasTelemetry = false;
    uint32_t batteryPercent = 0; // >100 conventionally means "powered/charging".
    float voltage = 0.0f;
    float channelUtilization = 0.0f;
    float airUtilTx = 0.0f;

    uint8_t hopLimit = 0;
    uint8_t hopStart = 0;
    float rssi = 0.0f;
    float snr = 0.0f;
    uint32_t lastSeenMs = 0;
    // Channel slot this node was last heard on; DMs to it go out on that
    // channel when we have no public key for it (what NodeDB does).
    uint8_t channel = 0;

    // Curve25519 public key from the node's NodeInfo (User.public_key).
    // Present means DMs to and from it use PKC rather than the channel PSK.
    bool hasPublicKey = false;
    uint8_t publicKey[32] = {0};
};

// Where a message we sent has got to. Mirrors the colour code Meshtastic's
// own device UI uses on message outlines: Acked green, Failed red, Relayed
// yellow (heard a relay rebroadcast it, no end-to-end ack yet).
enum class MeshtasticDelivery : uint8_t { None, Pending, Relayed, Acked, Failed };

constexpr uint32_t kMeshtasticBroadcast = 0xFFFFFFFFu;

// One channel slot, mirroring meshtastic ChannelSettings after key
// expansion: pskLen 0 = no encryption, 16 = AES-128, 32 = AES-256. The
// hash is what goes in the packet header's channel byte
// (Channels::generateHash: xorHash(name) ^ xorHash(expanded psk)).
struct MeshtasticChannel {
    bool used = false;
    char name[12] = {0}; // ChannelSettings.name is max 11 chars.
    uint8_t psk[32] = {0};
    uint8_t pskLen = 0;
    uint8_t hash = 0;
};

struct MeshtasticMessage {
    bool used = false;
    char text[160] = {0};
    uint32_t fromNum = 0;
    // Destination: kMeshtasticBroadcast for a channel message, a node
    // number for a direct message.
    uint32_t toNum = kMeshtasticBroadcast;
    uint32_t packetId = 0;
    // True for messages this watch originated.
    bool isOurs = false;
    // True when the DM was encrypted end-to-end with the peer's public key
    // rather than the shared channel PSK - what a lock icon should mean.
    bool pkiEncrypted = false;
    // Channel slot the message belongs to. Meaningful for channel messages;
    // for DMs it is whichever channel carried them (0 for PKC).
    uint8_t channel = 0;
    MeshtasticDelivery delivery = MeshtasticDelivery::None;
    float rssi = 0.0f;
    float snr = 0.0f;
    uint8_t hopLimit = 0;
    uint32_t receivedMs = 0;
};

struct MeshtasticStatus {
    // Raised from 8/6. Sized for a real mesh rather than a demo; the
    // structs are ~100 and ~200 bytes so this is ~25 KB of static RAM.
    static constexpr uint8_t kMaxNodes = 64;
    static constexpr uint8_t kMaxMessages = 96;
    static constexpr uint8_t kMaxChannels = 8; // Meshtastic MAX_NUM_CHANNELS.

    bool supported = false;
    bool radioEnabled = false;
    bool radioReady = false;
    int16_t radioError = 0;
    uint32_t packetCount = 0;
    uint32_t decodedCount = 0;
    float lastRssi = 0.0f;
    float lastSnr = 0.0f;
    uint16_t lastPacketBytes = 0;

    // Our own transmit identity/state - not a heard node, just what we tell
    // others about ourselves when advertisingEnabled is on.
    bool advertisingEnabled = false;
    uint32_t advertCount = 0;
    char longName[24] = {0};

    MeshtasticNode nodes[kMaxNodes];
    uint8_t nodeCount = 0;
    MeshtasticMessage messages[kMaxMessages];
    uint8_t messageCount = 0;
    // Slot 0 is always the primary LongFast channel and cannot be edited.
    MeshtasticChannel channels[kMaxChannels];
};

class MeshtasticService {
public:
    void begin();
    void poll();
    // Powers the SX1262 rail on/off and starts/stops receiving on the
    // US/LongFast frequency. Returns the resulting radioReady state.
    bool setRadioEnabled(bool enabled);
    // Sets our long_name (and a short_name derived from its first few
    // characters). Pass nullptr/empty to fall back to an auto-generated
    // name based on the chip's MAC. Safe to call any time, radio on or off.
    void setIdentity(const char *longName);
    // Whether we periodically transmit a NodeInfo advert announcing our
    // identity, mirroring MeshCore's MESH ADVERTISE. Independent of whether
    // sendTextMessage() works - you can chat without advertising, you'll
    // just show up to others as a bare node number until they hear a
    // NodeInfo from you some other way.
    void setAdvertisingEnabled(bool enabled);
    // Transmits a NodeInfo advert right now, bypassing the periodic timer.
    bool sendNodeInfoNow();
    // Transmits free text on the public LongFast channel. Requires the
    // radio to be enabled and ready; returns false otherwise.
    bool sendTextMessage(const char *text);
    // Direct message to one node. Sent with want_ack, so its delivery state
    // in the message list will move from Pending to Acked/Failed. End-to-end
    // encrypted with the peer's public key when we have heard one, else on
    // the channel the peer was last heard on.
    bool sendDirectMessage(uint32_t toNum, const char *text);
    // Free text on the channel in the given slot (sendTextMessage is slot 0).
    bool sendChannelMessage(uint8_t channelIndex, const char *text);
    // Adds or replaces the channel in a slot (1..kMaxChannels-1; slot 0 is
    // fixed). keyText is what the Meshtastic apps display for a channel's
    // PSK: a base64 string (16- or 32-byte key, or the 1-byte "simple key"
    // form like "AQ=="), or a bare number 0-10 (0 = no encryption, 1 =
    // the default key, 2-10 = the other simple keys). Persisted to NVS.
    bool setChannel(uint8_t index, const char *name, const char *keyText);
    bool removeChannel(uint8_t index);
    // Base64 text of a slot's key, for prefilling an editor.
    static void channelKeyText(const MeshtasticChannel &channel, char *out, size_t outSize);
    // First unused slot, or kMaxChannels if the table is full.
    uint8_t freeChannelSlot() const;
    // Broadcasts our own position / battery. Other nodes will not show us
    // on their maps or node lists with battery until we send these.
    bool sendPositionNow();
    bool sendTelemetryNow();
    // Fed by WatchApp each tick from GPS and the battery gauge; the periodic
    // broadcasts above read from these.
    void setOwnPosition(bool valid, double latitude, double longitude, int32_t altitudeM);
    void setOwnBattery(uint8_t percent);
    // Our last known position as fed by setOwnPosition(); false until a fix.
    bool ownPosition(double &latitude, double &longitude) const;
    uint32_t nodeNum() const { return _nodeNum; }
    const MeshtasticStatus &status() const { return _status; }

private:
    static void packetReceivedThunk();
    static volatile bool _packetReceived;

    void handlePacket(const uint8_t *raw, size_t len, float rssi, float snr);
    // PKC (firmware >= 2.5): X25519 shared secret -> SHA-256 -> AES-256-CCM,
    // 13-byte nonce, 8-byte tag, 4-byte random extra nonce appended. Header
    // channel byte is 0 for these instead of a channel hash.
    void loadOrCreateKeypair();
    void loadChannels();
    void saveChannel(uint8_t index);
    static bool parseChannelKey(const char *keyText, uint8_t *pskOut, uint8_t &pskLenOut);
    static uint8_t channelHash(const char *name, const uint8_t *psk, uint8_t pskLen);
    // AES-CTR with the slot's key, in place. A slot with pskLen 0 is a
    // plaintext channel and this is a no-op.
    void cryptChannel(uint8_t channelIndex, uint32_t packetId, uint32_t fromNum, uint8_t *data, size_t len) const;
    bool deriveSharedKey(const uint8_t peerPublic[32], uint8_t keyOut[32]) const;
    bool decryptPkc(uint32_t fromNum, uint32_t packetId, const uint8_t *peerPublic,
                    const uint8_t *in, size_t inLen, uint8_t *out, size_t &outLen) const;
    bool encryptPkc(uint32_t packetId, const uint8_t *peerPublic, const uint8_t *in, size_t inLen,
                    uint8_t *out, size_t &outLen) const;
    void handleData(uint32_t fromNum, uint32_t toNum, uint32_t packetId, bool wantAck,
                     uint8_t hopLimit, uint8_t hopStart, float rssi, float snr,
                     const uint8_t *decrypted, size_t len);
    void handleNodeInfo(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                         float rssi, float snr, const uint8_t *payload, size_t len);
    void handlePosition(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                         float rssi, float snr, const uint8_t *payload, size_t len);
    void handleTelemetry(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                          float rssi, float snr, const uint8_t *payload, size_t len);
    void handleTextMessage(uint32_t fromNum, uint32_t toNum, uint32_t packetId, uint8_t hopLimit,
                            float rssi, float snr, const uint8_t *payload, size_t len);
    void handleRouting(uint32_t fromNum, uint32_t requestId, const uint8_t *payload, size_t len);
    MeshtasticNode *findOrAllocateNode(uint32_t num);
    MeshtasticMessage *appendMessage();
    MeshtasticMessage *findOurMessage(uint32_t packetId);

    // Duplicate suppression. Every node in range rebroadcasts what it hears,
    // so one message arrives several times; without this each copy would
    // become its own entry in the list.
    bool seenPacket(uint32_t fromNum, uint32_t packetId);
    // Our own packet coming back from another node: implicit ack for a
    // broadcast, 'relayed' for a DM still waiting on its real ack.
    void noteRebroadcast(uint32_t packetId);
    // Ack goes back the way the packet came: PKC if it arrived PKC and we
    // hold the sender's key, otherwise on the same channel slot.
    bool sendAck(uint32_t toNum, uint32_t requestId, uint8_t channelIndex, const uint8_t *peerPublicKey);
    bool sendText(uint32_t toNum, uint8_t channelIndex, const char *text);
    void pollRetransmits(uint32_t nowMs);
    void pollPeriodicBroadcasts(uint32_t nowMs);

    // Builds the header, encrypts, transmits. packetIdOut receives the id
    // assigned, so a reliable send can be tracked and retried.
    // reliable: keep the encrypted packet in _pending and retry it until
    // acked (DM) or heard rebroadcast (broadcast). Off for position,
    // telemetry and acks, which are fire-and-forget.
    // peerPublicKey non-null selects the PKC path: channel byte 0, CCM with
    // the derived shared key, 12 bytes of overhead.
    bool transmitData(const uint8_t *plainPayload, size_t len, uint32_t toNum, bool wantAck,
                      uint32_t &packetIdOut, bool reliable = false,
                      const uint8_t *peerPublicKey = nullptr, uint8_t channelIndex = 0);
    bool transmitPacket(const uint8_t *data, size_t len);

    MeshtasticStatus _status;
    bool _keypairReady = false;
    // Set around handleData() for a packet that arrived PKC-encrypted, so
    // handleTextMessage can mark the message. Single-threaded, so a flag
    // rather than another parameter threaded through.
    bool _pkcInbound = false;
    // Channel slot the packet currently being handled arrived on.
    uint8_t _rxChannel = 0;
    uint8_t _privateKey[32] = {0};
    uint8_t _publicKey[32] = {0};
    uint8_t _messageWrite = 0;
    uint32_t _nodeNum = 0;
    char _shortName[8] = {0};
    uint32_t _lastAdvertMs = 0;
    uint32_t _lastPositionMs = 0;
    uint32_t _lastTelemetryMs = 0;
    static constexpr uint32_t kAdvertIntervalMs = 15UL * 60UL * 1000UL;
    // Meshtastic's own defaults are 15 min position / 30 min telemetry.
    static constexpr uint32_t kPositionIntervalMs = 15UL * 60UL * 1000UL;
    static constexpr uint32_t kTelemetryIntervalMs = 30UL * 60UL * 1000UL;

    bool _ownPositionValid = false;
    double _ownLatitude = 0.0;
    double _ownLongitude = 0.0;
    int32_t _ownAltitudeM = 0;
    uint8_t _ownBatteryPercent = 0;

    // Recently heard (from, id) pairs, for duplicate suppression.
    struct SeenPacket { uint32_t fromNum = 0; uint32_t packetId = 0; };
    SeenPacket _seen[32];
    uint8_t _seenWrite = 0;

    // Reliable sends awaiting an ack. The encrypted packet is kept as sent
    // so a retry is byte-identical, same id - that is what lets the
    // recipient dedupe it and still ack it.
    struct PendingTx {
        bool used = false;
        uint32_t packetId = 0;
        uint32_t toNum = 0;
        uint8_t attempts = 0;
        uint32_t lastSentMs = 0;
        size_t len = 0;
        uint8_t packet[256];
    };
    PendingTx _pending[4];
    // Meshtastic's ReliableRouter tries 3 times. The window is airtime
    // derived there; at SF11/250kHz a full packet is ~1 s on air, and a
    // multi-hop ack round trip a few seconds, so 10 s is comfortable.
    static constexpr uint8_t kMaxAttempts = 3;
    static constexpr uint32_t kRetransmitMs = 10000;
};
