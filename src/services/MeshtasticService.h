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
};

struct MeshtasticMessage {
    bool used = false;
    char text[160] = {0};
    uint32_t fromNum = 0;
    float rssi = 0.0f;
    float snr = 0.0f;
    uint8_t hopLimit = 0;
    uint32_t receivedMs = 0;
};

struct MeshtasticStatus {
    static constexpr uint8_t kMaxNodes = 8;
    static constexpr uint8_t kMaxMessages = 6;

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
    const MeshtasticStatus &status() const { return _status; }

private:
    static void packetReceivedThunk();
    static volatile bool _packetReceived;

    void handlePacket(const uint8_t *raw, size_t len, float rssi, float snr);
    void handleData(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                     float rssi, float snr, const uint8_t *decrypted, size_t len);
    void handleNodeInfo(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                         float rssi, float snr, const uint8_t *payload, size_t len);
    void handlePosition(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                         float rssi, float snr, const uint8_t *payload, size_t len);
    void handleTelemetry(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                          float rssi, float snr, const uint8_t *payload, size_t len);
    void handleTextMessage(uint32_t fromNum, uint8_t hopLimit, float rssi, float snr,
                            const uint8_t *payload, size_t len);
    MeshtasticNode *findOrAllocateNode(uint32_t num);

    bool transmitData(const uint8_t *plainPayload, size_t len);
    bool transmitPacket(const uint8_t *data, size_t len);

    MeshtasticStatus _status;
    uint8_t _messageWrite = 0;
    uint8_t _publicChannelHash = 0;
    uint32_t _nodeNum = 0;
    char _shortName[8] = {0};
    uint32_t _lastAdvertMs = 0;
    static constexpr uint32_t kAdvertIntervalMs = 15UL * 60UL * 1000UL;
};
