#include "MeshService.h"
#include "esp_mac.h"

#include <Arduino.h>
#include <LilyGoLib.h>
#include <RadioLib.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <esp_system.h>
#include <Preferences.h>
#include <ed25519.h>

namespace {
constexpr uint8_t kRouteMask = 0x03;
constexpr uint8_t kTypeShift = 2;
constexpr uint8_t kTypeMask = 0x0F;
constexpr uint8_t kPayloadAdvert = 0x04;
constexpr uint8_t kPayloadGroupText = 0x05;
constexpr uint8_t kRouteTransportFlood = 0x00;
constexpr uint8_t kRouteTransportDirect = 0x03;
constexpr uint8_t kPublicChannelHash = 0x11;

// MeshCore default public channel key. First 16 bytes are AES-128 key;
// MeshCore pads it to 32 bytes for HMAC-SHA256.
constexpr uint8_t kPublicSecret[32] = {
    0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
    0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

constexpr float kFrequencyMhz = 910.525f;
constexpr float kBandwidthKhz = 62.5f;
constexpr uint8_t kSpreadingFactor = 7;
constexpr uint8_t kCodingRate = 5;
constexpr uint8_t kSyncWord = 0x12;
constexpr int8_t kPowerDbm = 18;
constexpr uint16_t kPreambleSymbols = 16;
constexpr float kTcxoVoltage = 3.0f;

uint32_t readLe32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

void hmacSha256(const uint8_t *key, size_t keyLen, const uint8_t *data, size_t dataLen, uint8_t out[32])
{
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, key, keyLen);
    mbedtls_md_hmac_update(&ctx, data, dataLen);
    mbedtls_md_hmac_finish(&ctx, out);
    mbedtls_md_free(&ctx);
}

void aesEncrypt(const uint8_t *key, const uint8_t *in, uint8_t *out, size_t len)
{
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, 128);
    for (size_t i = 0; i < len; i += 16) {
        mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, in + i, out + i);
    }
    mbedtls_aes_free(&ctx);
}

void aesDecrypt(const uint8_t *key, const uint8_t *in, uint8_t *out, size_t len)
{
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, 128);
    for (size_t i = 0; i < len; i += 16) {
        mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, in + i, out + i);
    }
    mbedtls_aes_free(&ctx);
}
}

volatile bool MeshService::_packetReceived = false;

void MeshService::packetReceivedThunk()
{
    _packetReceived = true;
}

void MeshService::generateNodeName()
{
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    snprintf(_status.nodeName, sizeof(_status.nodeName), "LayerTime-%02X%02X", mac[4], mac[5]);
}

void MeshService::begin()
{
#if defined(ARDUINO_LILYGO_LORA_SX1262)
    _status.supported = true;
#else
    _status.supported = false;
#endif
    _status.radioEnabled = false;
    _status.radioReady = false;
    generateNodeName();
    loadOrCreateIdentity();
}

bool MeshService::setRadioEnabled(bool enabled)
{
#if defined(ARDUINO_LILYGO_LORA_SX1262)
    if (!_status.supported) {
        return false;
    }

    if (!enabled) {
        if (_status.radioEnabled) {
            // Stop listening and cut the SX1262 rail so it stops drawing power.
            radio.clearDio1Action();
            instance.powerControl(POWER_RADIO, false);
        }
        _status.radioEnabled = false;
        _status.radioReady = false;
        return false;
    }

    if (_status.radioEnabled && _status.radioReady) {
        // Already on.
        return true;
    }

    _status.radioEnabled = true;
    instance.powerControl(POWER_RADIO, true);
    delay(10); // Let the rail settle before talking SPI to the radio.

    const int16_t state = radio.begin(
        kFrequencyMhz, kBandwidthKhz, kSpreadingFactor, kCodingRate,
        kSyncWord, kPowerDbm, kPreambleSymbols, kTcxoVoltage, false);

    _status.radioError = state;
    if (state != RADIOLIB_ERR_NONE) {
        _status.radioReady = false;
        instance.powerControl(POWER_RADIO, false);
        _status.radioEnabled = false;
        return false;
    }

    radio.setCRC(1);
    radio.setDio2AsRfSwitch();
    radio.setCurrentLimit(140.0f);
    radio.setRxBoostedGainMode(true);
    radio.setDio1Action(packetReceivedThunk);

    const int16_t rxState = radio.startReceive();
    _status.radioError = rxState;
    _status.radioReady = (rxState == RADIOLIB_ERR_NONE);
    if (!_status.radioReady) {
        instance.powerControl(POWER_RADIO, false);
        _status.radioEnabled = false;
    }
    return _status.radioReady;
#else
    (void)enabled;
    _status.radioEnabled = false;
    _status.radioReady = false;
    return false;
#endif
}

void MeshService::poll()
{
#if defined(ARDUINO_LILYGO_LORA_SX1262)
    if (!_status.radioReady) return;

    const uint32_t nowMs = millis();
    if (_status.advertisingEnabled && (_lastAdvertMs == 0 || nowMs - _lastAdvertMs >= kAdvertIntervalMs)) {
        buildAndTransmitAdvert();
    }

    if (!_packetReceived) return;

    _packetReceived = false;
    const size_t packetLength = radio.getPacketLength();
    uint8_t buffer[256];
    const size_t readLength = packetLength > sizeof(buffer) ? sizeof(buffer) : packetLength;

    const int16_t state = radio.readData(buffer, readLength);
    if (state == RADIOLIB_ERR_NONE) {
        ++_status.packetCount;
        _status.lastPacketBytes = static_cast<uint16_t>(readLength);
        _status.lastRssi = radio.getRSSI();
        _status.lastSnr = radio.getSNR();
        parsePacket(buffer, readLength, _status.lastRssi, _status.lastSnr);
    } else {
        _status.radioError = state;
    }

    const int16_t rxState = radio.startReceive();
    if (rxState != RADIOLIB_ERR_NONE) {
        _status.radioReady = false;
        _status.radioError = rxState;
    }
#endif
}

void MeshService::parsePacket(const uint8_t *data, size_t len, float rssi, float snr)
{
    if (data == nullptr || len < 2) return;

    size_t offset = 0;
    const uint8_t header = data[offset++];
    const uint8_t routeType = header & kRouteMask;
    const uint8_t payloadType = (header >> kTypeShift) & kTypeMask;

    if (routeType == kRouteTransportFlood || routeType == kRouteTransportDirect) {
        if (offset + 4 > len) return;
        offset += 4;
    }

    if (offset >= len) return;
    const uint8_t packedPathLen = data[offset++];
    const uint8_t hopCount = packedPathLen & 0x3F;
    const uint8_t hashSize = (packedPathLen >> 6) + 1;
    const size_t pathBytes = static_cast<size_t>(hopCount) * hashSize;
    if (offset + pathBytes > len) return;
    offset += pathBytes;

    if (payloadType == kPayloadAdvert) {
        parseAdvert(data + offset, len - offset, rssi, snr);
    } else if (payloadType == kPayloadGroupText) {
        parseGroupText(data + offset, len - offset, rssi, hopCount);
    }
}

void MeshService::parseGroupText(const uint8_t *payload, size_t len, float rssi, uint8_t hops)
{
    if (payload == nullptr || len < 4 || payload[0] != kPublicChannelHash) return;

    const uint8_t *mac = payload + 1;
    const uint8_t *cipher = payload + 3;
    const size_t cipherLen = len - 3;
    if (cipherLen == 0 || cipherLen > 240 || (cipherLen % 16) != 0) return;

    uint8_t digest[32];
    hmacSha256(kPublicSecret, sizeof(kPublicSecret), cipher, cipherLen, digest);
    if (digest[0] != mac[0] || digest[1] != mac[1]) return;

    uint8_t plain[240] = {0};
    aesDecrypt(kPublicSecret, cipher, plain, cipherLen);
    if (cipherLen <= 5) return;

    char text[128];
    size_t n = 0;
    for (size_t i = 5; i < cipherLen && plain[i] != 0 && n < sizeof(text) - 1; ++i) {
        const uint8_t c = plain[i];
        text[n++] = (c >= 32 && c <= 126) ? static_cast<char>(c) : '?';
    }
    text[n] = '\0';
    if (n == 0) return;

    storeMessage(text, rssi, hops);
}

void MeshService::storeMessage(const char *text, float rssi, uint8_t hops)
{
    MeshMessage &msg = _status.messages[_messageWrite];
    msg = MeshMessage{};
    msg.used = true;
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.rssi = rssi;
    msg.hops = hops;
    msg.receivedMs = millis();
    _messageWrite = (_messageWrite + 1) % MeshStatus::kMaxMessages;
    ++_status.messageCount;
}

uint32_t MeshService::unixTimestamp() const
{
    struct tm now = {};
    instance.rtc.getDateTime(&now);
    return static_cast<uint32_t>(mktime(&now));
}

bool MeshService::transmitPacket(const uint8_t *data, size_t len)
{
#if defined(ARDUINO_LILYGO_LORA_SX1262)
    if (!_status.radioReady || data == nullptr || len == 0) return false;

    radio.clearDio1Action();
    const int16_t txState = radio.transmit(data, len);

    radio.setFrequency(910.525f);
    radio.setBandwidth(62.5f);
    radio.setSpreadingFactor(7);
    radio.setCodingRate(5);
    radio.setSyncWord(0x12);
    radio.setPreambleLength(16);
    radio.setCRC(1);
    radio.setDio2AsRfSwitch();
    radio.setDio1Action(packetReceivedThunk);
    const int16_t rxState = radio.startReceive();

    _status.radioError = (txState != RADIOLIB_ERR_NONE) ? txState : rxState;
    _status.radioReady = (rxState == RADIOLIB_ERR_NONE);
    return txState == RADIOLIB_ERR_NONE;
#else
    return false;
#endif
}

bool MeshService::sendPublicMessage(const char *text)
{
    if (text == nullptr || text[0] == '\0' || !_status.radioReady) return false;

    char named[150];
    snprintf(named, sizeof(named), "%s: %.110s", _status.nodeName, text);
    const size_t msgLen = strlen(named) + 1;

    uint8_t plain[160] = {0};
    const uint32_t ts = unixTimestamp();
    memcpy(plain, &ts, 4);
    plain[4] = 0x00;
    memcpy(plain + 5, named, msgLen);
    size_t plainLen = 5 + msgLen;
    const size_t paddedLen = (plainLen + 15) & ~static_cast<size_t>(15);
    if (paddedLen > sizeof(plain)) return false;

    uint8_t cipher[160] = {0};
    aesEncrypt(kPublicSecret, plain, cipher, paddedLen);
    uint8_t digest[32];
    hmacSha256(kPublicSecret, sizeof(kPublicSecret), cipher, paddedLen, digest);

    uint8_t packet[180];
    size_t pos = 0;
    packet[pos++] = 0x15; // Flood + group text
    packet[pos++] = 0x00; // zero-hop path metadata at origin
    packet[pos++] = kPublicChannelHash;
    packet[pos++] = digest[0];
    packet[pos++] = digest[1];
    memcpy(packet + pos, cipher, paddedLen);
    pos += paddedLen;

    const bool ok = transmitPacket(packet, pos);
    if (ok) storeMessage(named, 0.0f, 0);
    return ok;
}

void MeshService::loadOrCreateIdentity()
{
    if (_identityReady) return;

    Preferences prefs;
    bool loaded = false;
    if (prefs.begin("meshcore", true)) {
        if (prefs.getBytesLength("sk") == sizeof(_privateKey) &&
            prefs.getBytesLength("pk") == sizeof(_publicKey) &&
            prefs.getUChar("kv", 0) == 2) {
            prefs.getBytes("sk", _privateKey, sizeof(_privateKey));
            prefs.getBytes("pk", _publicKey, sizeof(_publicKey));
            loaded = true;
        }
        prefs.end();
    }

    if (!loaded) {
        uint8_t seed[32];
        esp_fill_random(seed, sizeof(seed));
        ed25519_create_keypair(_publicKey, _privateKey, seed);

        if (prefs.begin("meshcore", false)) {
            prefs.putBytes("sk", _privateKey, sizeof(_privateKey));
            prefs.putBytes("pk", _publicKey, sizeof(_publicKey));
            prefs.putUChar("kv", 2);
            prefs.end();
        }
    }

    _identityReady = true;
}

void MeshService::setAdvertisingEnabled(bool enabled)
{
    if (enabled == _status.advertisingEnabled) {
        return;
    }
    _status.advertisingEnabled = enabled;
    if (enabled) {
        _lastAdvertMs = 0;
    }
}

bool MeshService::sendAdvertNow()
{
    if (!_status.radioReady) return false;
    return buildAndTransmitAdvert();
}

bool MeshService::buildAndTransmitAdvert()
{
    if (!_status.radioReady || !_identityReady) return false;

    uint8_t appdata[64] = {0};
    size_t appLen = 0;
    uint8_t flags = 0x01 | 0x80;

    if (instance.gps.location.isValid()) flags |= 0x10;
    appdata[appLen++] = flags;

    if ((flags & 0x10) != 0) {
        const int32_t latE6 = static_cast<int32_t>(instance.gps.location.lat() * 1000000.0);
        const int32_t lonE6 = static_cast<int32_t>(instance.gps.location.lng() * 1000000.0);
        memcpy(appdata + appLen, &latE6, 4); appLen += 4;
        memcpy(appdata + appLen, &lonE6, 4); appLen += 4;
    }

    const size_t nameLen = strnlen(_status.nodeName, sizeof(_status.nodeName) - 1);
    memcpy(appdata + appLen, _status.nodeName, nameLen); appLen += nameLen;
    appdata[appLen++] = 0;

    const uint32_t ts = unixTimestamp();
    uint8_t signData[128] = {0};
    memcpy(signData, _publicKey, 32);
    memcpy(signData + 32, &ts, 4);
    memcpy(signData + 36, appdata, appLen);
    const size_t signLen = 36 + appLen;

    uint8_t signature[64] = {0};
    ed25519_sign(signature, signData, signLen, _publicKey, _privateKey);

    uint8_t packet[180] = {0};
    size_t pos = 0;
    packet[pos++] = 0x11;
    packet[pos++] = 0x00;
    memcpy(packet + pos, _publicKey, 32); pos += 32;
    memcpy(packet + pos, &ts, 4); pos += 4;
    memcpy(packet + pos, signature, 64); pos += 64;
    memcpy(packet + pos, appdata, appLen); pos += appLen;

    const bool ok = transmitPacket(packet, pos);
    if (ok) _lastAdvertMs = millis();
    return ok;
}

void MeshService::parseAdvert(const uint8_t *payload, size_t len, float rssi, float snr)
{
    if (payload == nullptr || len < 101) return;

    const uint8_t *pubKey = payload;
    size_t offset = 32 + 4 + 64;
    const uint8_t flags = payload[offset++];

    MeshNode *node = findOrAllocateNode(pubKey);
    if (node == nullptr) return;

    node->type = flags & 0x0F;
    node->rssi = rssi;
    node->snr = snr;
    node->lastSeenMs = millis();
    node->hasLocation = false;

    if ((flags & 0x10) != 0) {
        if (offset + 8 > len) return;
        const int32_t latE6 = static_cast<int32_t>(readLe32(payload + offset));
        offset += 4;
        const int32_t lonE6 = static_cast<int32_t>(readLe32(payload + offset));
        offset += 4;
        node->latitude = static_cast<double>(latE6) / 1000000.0;
        node->longitude = static_cast<double>(lonE6) / 1000000.0;
        node->hasLocation = true;
    }

    if ((flags & 0x20) != 0) { if (offset + 2 > len) return; offset += 2; }
    if ((flags & 0x40) != 0) { if (offset + 2 > len) return; offset += 2; }

    if ((flags & 0x80) != 0 && offset < len) {
        const size_t available = len - offset;
        const size_t copyLen = available < sizeof(node->name) - 1 ? available : sizeof(node->name) - 1;
        memcpy(node->name, payload + offset, copyLen);
        node->name[copyLen] = '\0';
        for (size_t i = 0; i < copyLen; ++i) {
            const unsigned char c = static_cast<unsigned char>(node->name[i]);
            if (c < 32 || c > 126) node->name[i] = '?';
        }
    }

    if (node->name[0] == '\0') {
        snprintf(node->name, sizeof(node->name), "%02X%02X%02X%02X",
            node->id[0], node->id[1], node->id[2], node->id[3]);
    }

    ++_status.advertCount;
}

MeshNode *MeshService::findOrAllocateNode(const uint8_t *pubKey)
{
    for (uint8_t i = 0; i < MeshStatus::kMaxNodes; ++i) {
        MeshNode &node = _status.nodes[i];
        if (node.used && memcmp(node.id, pubKey, sizeof(node.id)) == 0) return &node;
    }

    for (uint8_t i = 0; i < MeshStatus::kMaxNodes; ++i) {
        MeshNode &node = _status.nodes[i];
        if (!node.used) {
            node.used = true;
            memcpy(node.id, pubKey, sizeof(node.id));
            if (_status.nodeCount < MeshStatus::kMaxNodes) ++_status.nodeCount;
            return &node;
        }
    }

    uint8_t stalest = 0;
    for (uint8_t i = 1; i < MeshStatus::kMaxNodes; ++i) {
        if (_status.nodes[i].lastSeenMs < _status.nodes[stalest].lastSeenMs) stalest = i;
    }
    MeshNode &node = _status.nodes[stalest];
    node = MeshNode{};
    node.used = true;
    memcpy(node.id, pubKey, sizeof(node.id));
    return &node;
}
