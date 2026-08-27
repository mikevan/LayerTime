#pragma once

#include <stdint.h>
#include <stddef.h>

struct MeshNode {
    bool used = false;
    uint8_t id[4] = {0, 0, 0, 0};
    char name[25] = {0};
    uint8_t type = 0;
    bool hasLocation = false;
    double latitude = 0.0;
    double longitude = 0.0;
    float rssi = 0.0f;
    float snr = 0.0f;
    uint32_t lastSeenMs = 0;
};

struct MeshMessage {
    bool used = false;
    char text[128] = {0};
    float rssi = 0.0f;
    uint8_t hops = 0;
    uint32_t receivedMs = 0;
};

struct MeshStatus {
    static constexpr uint8_t kMaxNodes = 8;
    static constexpr uint8_t kMaxMessages = 6;

    bool supported = false;
    // True once the user has opted the radio on this session (Settings > MESH).
    bool radioEnabled = false;
    // True once the radio has actually powered up and is receiving.
    bool radioReady = false;
    bool advertisingEnabled = false;
    int16_t radioError = 0;
    uint32_t packetCount = 0;
    uint32_t advertCount = 0;
    uint32_t messageCount = 0;
    float lastRssi = 0.0f;
    float lastSnr = 0.0f;
    uint16_t lastPacketBytes = 0;
    char nodeName[24] = {0};
    MeshNode nodes[kMaxNodes];
    uint8_t nodeCount = 0;
    MeshMessage messages[kMaxMessages];
};

class MeshService {
public:
    // Identity/name setup only. Cheap, no radio power - safe to call every boot.
    void begin();
    void poll();
    bool sendPublicMessage(const char *text);
    void setAdvertisingEnabled(bool enabled);
    bool sendAdvertNow();
    // Powers the SX1262 rail on/off and starts/stops receiving. Returns the
    // resulting radioReady state.
    bool setRadioEnabled(bool enabled);
    const MeshStatus &status() const { return _status; }

private:
    static void packetReceivedThunk();
    static volatile bool _packetReceived;

    void parsePacket(const uint8_t *data, size_t len, float rssi, float snr);
    void parseAdvert(const uint8_t *payload, size_t len, float rssi, float snr);
    void parseGroupText(const uint8_t *payload, size_t len, float rssi, uint8_t hops);
    MeshNode *findOrAllocateNode(const uint8_t *pubKey);
    void storeMessage(const char *text, float rssi, uint8_t hops);
    bool transmitPacket(const uint8_t *data, size_t len);
    uint32_t unixTimestamp() const;
    void generateNodeName();
    void loadOrCreateIdentity();
    bool buildAndTransmitAdvert();

    MeshStatus _status;
    uint8_t _messageWrite = 0;
    uint8_t _publicKey[32] = {0};
    uint8_t _privateKey[64] = {0};
    bool _identityReady = false;
    uint32_t _lastAdvertMs = 0;
    static constexpr uint32_t kAdvertIntervalMs = 15UL * 60UL * 1000UL;
};
