#include "MeshtasticService.h"
#include "esp_mac.h"

#include <Arduino.h>
#include <LilyGoLib.h>
#include <RadioLib.h>
#include <mbedtls/aes.h>
#include <esp_system.h>
#include <esp_random.h>
#include <stdio.h>
#include <string.h>

// Passive Meshtastic decoder. Wire-format facts below (radio params, packet
// header layout, default channel PSK, AES-CTR nonce construction, protobuf
// field numbers) were verified directly against meshtastic/firmware and
// meshtastic/protobufs source, not guessed - see inline citations.

namespace {
// US region, LongFast modem preset - matches the out-of-box default most
// Meshtastic devices (including a stock Cardputer ADV) run on.
// meshtastic/firmware src/mesh/MeshRadio.h (modemPresetToParams, LONG_FAST
// branch) + src/mesh/RadioInterface.cpp (US region table).
constexpr float kFrequencyMhz = 906.875f; // computed from the US region table + LongFast channel-hash formula.
constexpr float kBandwidthKhz = 250.0f;
constexpr uint8_t kSpreadingFactor = 11;
constexpr uint8_t kCodingRate = 5; // RadioLib coding-rate denominator (4/5).
constexpr uint8_t kSyncWord = 0x2b;
constexpr int8_t kPowerDbm = 17; // Unused for RX-only, but must be a legal value for radio.begin().
constexpr uint16_t kPreambleSymbols = 16;
constexpr float kTcxoVoltage = 3.0f;
constexpr uint8_t kDefaultHopLimit = 3; // Meshtastic's stock default hop_limit/hop_start for originated packets.

constexpr const char *kDefaultChannelName = "LongFast";

// 16-byte AES-128 key all stock devices power up on for the default public
// channel. meshtastic/firmware src/mesh/Channels.h (defaultpsk).
constexpr uint8_t kDefaultPsk[16] = {
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
    0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
};

constexpr size_t kHeaderLen = 16;
constexpr size_t kMaxPayloadLen = 240;

// meshtastic/protobufs meshtastic/portnums.proto
constexpr uint32_t kPortTextMessage = 1;
constexpr uint32_t kPortPosition = 3;
constexpr uint32_t kPortNodeInfo = 4;
constexpr uint32_t kPortTelemetry = 67;

uint32_t readLe32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

int32_t readFixed32Signed(const uint8_t *p)
{
    return static_cast<int32_t>(readLe32(p));
}

float readFixed32Float(const uint8_t *p)
{
    const uint32_t u = readLe32(p);
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

uint8_t xorHash(const uint8_t *data, size_t len)
{
    uint8_t h = 0;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
    }
    return h;
}

// Minimal protobuf wire-format primitives - just enough to read the handful
// of fields we care about from the message types Meshtastic sends over the
// air. No nanopb/protoc toolchain is available in this build environment.
bool readVarint(const uint8_t *data, size_t len, size_t &offset, uint64_t &value)
{
    value = 0;
    int shift = 0;
    while (offset < len) {
        const uint8_t b = data[offset++];
        value |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) {
            return true;
        }
        shift += 7;
        if (shift > 63) {
            return false;
        }
    }
    return false;
}

bool skipField(const uint8_t *data, size_t len, size_t &offset, uint8_t wireType)
{
    switch (wireType) {
        case 0: { // varint
            uint64_t v;
            return readVarint(data, len, offset, v);
        }
        case 1: // fixed64
            if (offset + 8 > len) return false;
            offset += 8;
            return true;
        case 2: { // length-delimited
            uint64_t l;
            if (!readVarint(data, len, offset, l)) return false;
            if (offset + l > len) return false;
            offset += l;
            return true;
        }
        case 5: // fixed32
            if (offset + 4 > len) return false;
            offset += 4;
            return true;
        default:
            return false;
    }
}

// --- Write-side protobuf primitives, mirroring the read-side ones above. ---
size_t writeVarint(uint8_t *buf, size_t pos, uint64_t value)
{
    while (value >= 0x80) {
        buf[pos++] = static_cast<uint8_t>(value) | 0x80;
        value >>= 7;
    }
    buf[pos++] = static_cast<uint8_t>(value);
    return pos;
}

size_t writeTag(uint8_t *buf, size_t pos, uint32_t fieldNum, uint8_t wireType)
{
    return writeVarint(buf, pos, (static_cast<uint64_t>(fieldNum) << 3) | wireType);
}

size_t writeLengthDelimited(uint8_t *buf, size_t pos, uint32_t fieldNum, const uint8_t *data, size_t len)
{
    pos = writeTag(buf, pos, fieldNum, 2);
    pos = writeVarint(buf, pos, len);
    memcpy(buf + pos, data, len);
    return pos + len;
}

size_t writeStringField(uint8_t *buf, size_t pos, uint32_t fieldNum, const char *str)
{
    return writeLengthDelimited(buf, pos, fieldNum, reinterpret_cast<const uint8_t *>(str), strlen(str));
}

size_t writeVarintField(uint8_t *buf, size_t pos, uint32_t fieldNum, uint64_t value)
{
    pos = writeTag(buf, pos, fieldNum, 0);
    return writeVarint(buf, pos, value);
}
}

volatile bool MeshtasticService::_packetReceived = false;

void MeshtasticService::packetReceivedThunk()
{
    _packetReceived = true;
}

void MeshtasticService::begin()
{
#if defined(ARDUINO_LILYGO_LORA_SX1262)
    _status.supported = true;
#else
    _status.supported = false;
#endif
    _status.radioEnabled = false;
    _status.radioReady = false;

    // meshtastic/firmware src/mesh/Channels.cpp: channel hash = xorHash(name) ^ xorHash(psk).
    _publicChannelHash = xorHash(reinterpret_cast<const uint8_t *>(kDefaultChannelName), strlen(kDefaultChannelName)) ^
                          xorHash(kDefaultPsk, sizeof(kDefaultPsk));

    // Synthesize a NodeNum for ourselves from the chip's factory MAC, same
    // general idea real Meshtastic devices use. We're not a registered
    // device so this is only ever used as our "from" field when we transmit.
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    _nodeNum = (static_cast<uint32_t>(mac[2]) << 24) | (static_cast<uint32_t>(mac[3]) << 16) |
               (static_cast<uint32_t>(mac[4]) << 8) | static_cast<uint32_t>(mac[5]);

    setIdentity(nullptr); // Sensible default until WatchApp supplies the configured name.
}

void MeshtasticService::setIdentity(const char *longName)
{
    if (longName == nullptr || longName[0] == '\0') {
        snprintf(_status.longName, sizeof(_status.longName), "LayerTime-%04X", static_cast<unsigned>(_nodeNum & 0xFFFF));
    } else {
        strncpy(_status.longName, longName, sizeof(_status.longName) - 1);
        _status.longName[sizeof(_status.longName) - 1] = '\0';
    }

    // Meshtastic short_name convention is a handful of glyphs - derive from
    // the front of the long name so there's only one name to configure.
    const size_t n = strnlen(_status.longName, sizeof(_shortName) - 1);
    memcpy(_shortName, _status.longName, n);
    _shortName[n] = '\0';
}

void MeshtasticService::setAdvertisingEnabled(bool enabled)
{
    if (enabled == _status.advertisingEnabled) {
        return;
    }
    _status.advertisingEnabled = enabled;
    if (enabled) {
        _lastAdvertMs = 0; // Trigger an immediate advert on the next poll().
    }
}

bool MeshtasticService::setRadioEnabled(bool enabled)
{
#if defined(ARDUINO_LILYGO_LORA_SX1262)
    if (!_status.supported) {
        return false;
    }

    if (!enabled) {
        if (_status.radioEnabled) {
            radio.clearDio1Action();
            instance.powerControl(POWER_RADIO, false);
        }
        _status.radioEnabled = false;
        _status.radioReady = false;
        return false;
    }

    if (_status.radioEnabled && _status.radioReady) {
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

void MeshtasticService::poll()
{
#if defined(ARDUINO_LILYGO_LORA_SX1262)
    if (!_status.radioReady) return;

    const uint32_t nowMs = millis();
    if (_status.advertisingEnabled && (_lastAdvertMs == 0 || nowMs - _lastAdvertMs >= kAdvertIntervalMs)) {
        sendNodeInfoNow();
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
        handlePacket(buffer, readLength, _status.lastRssi, _status.lastSnr);
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

void MeshtasticService::handlePacket(const uint8_t *raw, size_t len, float rssi, float snr)
{
    if (raw == nullptr || len < kHeaderLen) return;

    // meshtastic/firmware src/mesh/RadioInterface.h PacketHeader - 16 bytes,
    // native little-endian, no framing/escaping.
    const uint32_t fromNum = readLe32(raw + 4);
    const uint32_t packetId = readLe32(raw + 8);
    const uint8_t flags = raw[12];
    const uint8_t channelHash = raw[13];

    if (channelHash != _publicChannelHash) {
        return; // Not the default public channel/key - can't decrypt this one.
    }

    const uint8_t hopLimit = flags & 0x07;
    const uint8_t hopStart = (flags >> 5) & 0x07;

    const size_t payloadLen = len - kHeaderLen;
    if (payloadLen == 0 || payloadLen > kMaxPayloadLen) return;

    uint8_t decrypted[kMaxPayloadLen];
    memcpy(decrypted, raw + kHeaderLen, payloadLen);

    // meshtastic/firmware src/mesh/CryptoEngine.cpp initNonce(): 16-byte
    // nonce = packetId as zero-extended LE64 (bytes 0-7) || fromNode as LE32
    // (bytes 8-11) || zero (bytes 12-15), for the no-extraNonce (public
    // channel) path. Key is exactly 16 bytes here -> AES-128-CTR.
    uint8_t nonce[16] = {0};
    nonce[0] = static_cast<uint8_t>(packetId);
    nonce[1] = static_cast<uint8_t>(packetId >> 8);
    nonce[2] = static_cast<uint8_t>(packetId >> 16);
    nonce[3] = static_cast<uint8_t>(packetId >> 24);
    nonce[8] = static_cast<uint8_t>(fromNum);
    nonce[9] = static_cast<uint8_t>(fromNum >> 8);
    nonce[10] = static_cast<uint8_t>(fromNum >> 16);
    nonce[11] = static_cast<uint8_t>(fromNum >> 24);

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, kDefaultPsk, 128); // CTR always uses the encrypt key schedule.
    size_t ncOff = 0;
    uint8_t streamBlock[16] = {0};
    mbedtls_aes_crypt_ctr(&ctx, payloadLen, &ncOff, nonce, streamBlock, decrypted, decrypted);
    mbedtls_aes_free(&ctx);

    handleData(fromNum, hopLimit, hopStart, rssi, snr, decrypted, payloadLen);
}

void MeshtasticService::handleData(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                                    float rssi, float snr, const uint8_t *decrypted, size_t len)
{
    // meshtastic/protobufs meshtastic/mesh.proto Data: 1=portnum(varint), 2=payload(bytes).
    size_t offset = 0;
    uint32_t portnum = 0;
    const uint8_t *payload = nullptr;
    size_t payloadLen = 0;

    while (offset < len) {
        uint64_t tag;
        if (!readVarint(decrypted, len, offset, tag)) break;
        const uint32_t fieldNum = static_cast<uint32_t>(tag >> 3);
        const uint8_t wireType = static_cast<uint8_t>(tag & 0x07);

        if (fieldNum == 1 && wireType == 0) {
            uint64_t v;
            if (!readVarint(decrypted, len, offset, v)) break;
            portnum = static_cast<uint32_t>(v);
        } else if (fieldNum == 2 && wireType == 2) {
            uint64_t l;
            if (!readVarint(decrypted, len, offset, l)) break;
            if (offset + l > len) break;
            payload = decrypted + offset;
            payloadLen = static_cast<size_t>(l);
            offset += l;
        } else {
            if (!skipField(decrypted, len, offset, wireType)) break;
        }
    }

    if (payload == nullptr) {
        // Failed to decode as a Data message at all - almost always means the
        // decrypt key/nonce didn't line up (e.g. a non-default channel that
        // happened to share our channel hash byte). Not worth surfacing as
        // an error; just drop it.
        return;
    }

    ++_status.decodedCount;

    switch (portnum) {
        case kPortTextMessage:
            handleTextMessage(fromNum, hopLimit, rssi, snr, payload, payloadLen);
            break;
        case kPortPosition:
            handlePosition(fromNum, hopLimit, hopStart, rssi, snr, payload, payloadLen);
            break;
        case kPortNodeInfo:
            handleNodeInfo(fromNum, hopLimit, hopStart, rssi, snr, payload, payloadLen);
            break;
        case kPortTelemetry:
            handleTelemetry(fromNum, hopLimit, hopStart, rssi, snr, payload, payloadLen);
            break;
        default:
            break;
    }
}

void MeshtasticService::handleNodeInfo(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                                        float rssi, float snr, const uint8_t *payload, size_t len)
{
    // NODEINFO_APP payload is a bare User message (meshtastic/protobufs
    // portnums.proto), not the NodeInfo wrapper - that wrapper is
    // phone-API-only and never sent over the radio.
    MeshtasticNode *node = findOrAllocateNode(fromNum);
    if (node == nullptr) return;
    node->hopLimit = hopLimit;
    node->hopStart = hopStart;
    node->rssi = rssi;
    node->snr = snr;
    node->lastSeenMs = millis();

    // User: 2=long_name(string), 3=short_name(string).
    size_t offset = 0;
    while (offset < len) {
        uint64_t tag;
        if (!readVarint(payload, len, offset, tag)) break;
        const uint32_t fieldNum = static_cast<uint32_t>(tag >> 3);
        const uint8_t wireType = static_cast<uint8_t>(tag & 0x07);

        if (fieldNum == 2 && wireType == 2) {
            uint64_t l;
            if (!readVarint(payload, len, offset, l)) break;
            if (offset + l > len) break;
            const size_t n = l < sizeof(node->longName) - 1 ? static_cast<size_t>(l) : sizeof(node->longName) - 1;
            memcpy(node->longName, payload + offset, n);
            node->longName[n] = '\0';
            offset += l;
        } else if (fieldNum == 3 && wireType == 2) {
            uint64_t l;
            if (!readVarint(payload, len, offset, l)) break;
            if (offset + l > len) break;
            const size_t n = l < sizeof(node->shortName) - 1 ? static_cast<size_t>(l) : sizeof(node->shortName) - 1;
            memcpy(node->shortName, payload + offset, n);
            node->shortName[n] = '\0';
            offset += l;
        } else {
            if (!skipField(payload, len, offset, wireType)) break;
        }
    }
}

void MeshtasticService::handlePosition(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                                        float rssi, float snr, const uint8_t *payload, size_t len)
{
    MeshtasticNode *node = findOrAllocateNode(fromNum);
    if (node == nullptr) return;
    node->hopLimit = hopLimit;
    node->hopStart = hopStart;
    node->rssi = rssi;
    node->snr = snr;
    node->lastSeenMs = millis();

    // Position: 1=latitude_i(sfixed32), 2=longitude_i(sfixed32), 3=altitude(int32).
    // sfixed32 is a raw little-endian 32-bit two's-complement value - NOT
    // zigzag-encoded (that would be sint32, a different field type).
    size_t offset = 0;
    bool haveLat = false;
    bool haveLon = false;
    int32_t latI = 0;
    int32_t lonI = 0;

    while (offset < len) {
        uint64_t tag;
        if (!readVarint(payload, len, offset, tag)) break;
        const uint32_t fieldNum = static_cast<uint32_t>(tag >> 3);
        const uint8_t wireType = static_cast<uint8_t>(tag & 0x07);

        if (fieldNum == 1 && wireType == 5) {
            if (offset + 4 > len) break;
            latI = readFixed32Signed(payload + offset);
            offset += 4;
            haveLat = true;
        } else if (fieldNum == 2 && wireType == 5) {
            if (offset + 4 > len) break;
            lonI = readFixed32Signed(payload + offset);
            offset += 4;
            haveLon = true;
        } else if (fieldNum == 3 && wireType == 0) {
            uint64_t v;
            if (!readVarint(payload, len, offset, v)) break;
            node->altitudeM = static_cast<int32_t>(v);
        } else {
            if (!skipField(payload, len, offset, wireType)) break;
        }
    }

    if (haveLat && haveLon) {
        node->latitude = static_cast<double>(latI) * 1e-7;
        node->longitude = static_cast<double>(lonI) * 1e-7;
        node->hasPosition = true;
    }
}

void MeshtasticService::handleTelemetry(uint32_t fromNum, uint8_t hopLimit, uint8_t hopStart,
                                         float rssi, float snr, const uint8_t *payload, size_t len)
{
    MeshtasticNode *node = findOrAllocateNode(fromNum);
    if (node == nullptr) return;
    node->hopLimit = hopLimit;
    node->hopStart = hopStart;
    node->rssi = rssi;
    node->snr = snr;
    node->lastSeenMs = millis();

    // Telemetry: 2=device_metrics(submessage, oneof).
    size_t offset = 0;
    const uint8_t *metrics = nullptr;
    size_t metricsLen = 0;

    while (offset < len) {
        uint64_t tag;
        if (!readVarint(payload, len, offset, tag)) break;
        const uint32_t fieldNum = static_cast<uint32_t>(tag >> 3);
        const uint8_t wireType = static_cast<uint8_t>(tag & 0x07);

        if (fieldNum == 2 && wireType == 2) {
            uint64_t l;
            if (!readVarint(payload, len, offset, l)) break;
            if (offset + l > len) break;
            metrics = payload + offset;
            metricsLen = static_cast<size_t>(l);
            offset += l;
        } else {
            if (!skipField(payload, len, offset, wireType)) break;
        }
    }

    if (metrics == nullptr) return;

    // DeviceMetrics: 1=battery_level(varint), 2=voltage(float), 3=channel_utilization(float), 4=air_util_tx(float).
    size_t mOffset = 0;
    bool any = false;
    while (mOffset < metricsLen) {
        uint64_t tag;
        if (!readVarint(metrics, metricsLen, mOffset, tag)) break;
        const uint32_t fieldNum = static_cast<uint32_t>(tag >> 3);
        const uint8_t wireType = static_cast<uint8_t>(tag & 0x07);

        if (fieldNum == 1 && wireType == 0) {
            uint64_t v;
            if (!readVarint(metrics, metricsLen, mOffset, v)) break;
            node->batteryPercent = static_cast<uint32_t>(v);
            any = true;
        } else if (fieldNum == 2 && wireType == 5) {
            if (mOffset + 4 > metricsLen) break;
            node->voltage = readFixed32Float(metrics + mOffset);
            mOffset += 4;
            any = true;
        } else if (fieldNum == 3 && wireType == 5) {
            if (mOffset + 4 > metricsLen) break;
            node->channelUtilization = readFixed32Float(metrics + mOffset);
            mOffset += 4;
            any = true;
        } else if (fieldNum == 4 && wireType == 5) {
            if (mOffset + 4 > metricsLen) break;
            node->airUtilTx = readFixed32Float(metrics + mOffset);
            mOffset += 4;
            any = true;
        } else {
            if (!skipField(metrics, metricsLen, mOffset, wireType)) break;
        }
    }

    if (any) {
        node->hasTelemetry = true;
    }
}

void MeshtasticService::handleTextMessage(uint32_t fromNum, uint8_t hopLimit, float rssi, float snr,
                                           const uint8_t *payload, size_t len)
{
    // TEXT_MESSAGE_APP payload is raw UTF-8 bytes, not further protobuf-wrapped.
    MeshtasticMessage &msg = _status.messages[_messageWrite];
    msg = MeshtasticMessage{};
    msg.used = true;
    msg.fromNum = fromNum;
    msg.rssi = rssi;
    msg.snr = snr;
    msg.hopLimit = hopLimit;
    msg.receivedMs = millis();

    const size_t n = len < sizeof(msg.text) - 1 ? len : sizeof(msg.text) - 1;
    memcpy(msg.text, payload, n);
    msg.text[n] = '\0';

    _messageWrite = (_messageWrite + 1) % MeshtasticStatus::kMaxMessages;
    if (_status.messageCount < MeshtasticStatus::kMaxMessages) {
        ++_status.messageCount;
    }
}

MeshtasticNode *MeshtasticService::findOrAllocateNode(uint32_t num)
{
    for (uint8_t i = 0; i < _status.nodeCount; ++i) {
        if (_status.nodes[i].used && _status.nodes[i].num == num) {
            return &_status.nodes[i];
        }
    }

    if (_status.nodeCount < MeshtasticStatus::kMaxNodes) {
        MeshtasticNode &node = _status.nodes[_status.nodeCount];
        node = MeshtasticNode{};
        node.used = true;
        node.num = num;
        ++_status.nodeCount;
        return &node;
    }

    // Full - evict the least-recently-seen node rather than dropping the new one.
    uint8_t oldestIndex = 0;
    uint32_t oldestMs = _status.nodes[0].lastSeenMs;
    for (uint8_t i = 1; i < MeshtasticStatus::kMaxNodes; ++i) {
        if (_status.nodes[i].lastSeenMs < oldestMs) {
            oldestMs = _status.nodes[i].lastSeenMs;
            oldestIndex = i;
        }
    }
    MeshtasticNode &node = _status.nodes[oldestIndex];
    node = MeshtasticNode{};
    node.used = true;
    node.num = num;
    return &node;
}

bool MeshtasticService::sendTextMessage(const char *text)
{
    if (text == nullptr || text[0] == '\0' || !_status.radioReady) return false;

    const size_t textLen = strlen(text);
    if (textLen > 200) return false; // Stay well under kMaxPayloadLen once the Data wrapper is added.

    // Data: 1=portnum(varint), 2=payload(bytes). meshtastic/protobufs mesh.proto.
    uint8_t dataMsg[220];
    size_t pos = 0;
    pos = writeVarintField(dataMsg, pos, 1, kPortTextMessage);
    pos = writeLengthDelimited(dataMsg, pos, 2, reinterpret_cast<const uint8_t *>(text), textLen);

    const bool ok = transmitData(dataMsg, pos);
    if (ok) {
        // Mirror our own send into the message list so CHAT shows it (0
        // rssi marks a locally-originated message, same convention MeshCore
        // uses for its own sent messages).
        MeshtasticMessage &msg = _status.messages[_messageWrite];
        msg = MeshtasticMessage{};
        msg.used = true;
        msg.fromNum = _nodeNum;
        msg.rssi = 0.0f;
        msg.snr = 0.0f;
        msg.hopLimit = kDefaultHopLimit;
        msg.receivedMs = millis();
        const size_t n = textLen < sizeof(msg.text) - 1 ? textLen : sizeof(msg.text) - 1;
        memcpy(msg.text, text, n);
        msg.text[n] = '\0';

        _messageWrite = (_messageWrite + 1) % MeshtasticStatus::kMaxMessages;
        if (_status.messageCount < MeshtasticStatus::kMaxMessages) {
            ++_status.messageCount;
        }
    }
    return ok;
}

bool MeshtasticService::sendNodeInfoNow()
{
    if (!_status.radioReady) return false;

    // User: 2=long_name(string), 3=short_name(string). meshtastic/protobufs mesh.proto.
    uint8_t user[80];
    size_t upos = 0;
    upos = writeStringField(user, upos, 2, _status.longName);
    upos = writeStringField(user, upos, 3, _shortName);

    // Data: 1=portnum(varint), 2=payload(bytes), wrapping the User message above.
    uint8_t dataMsg[120];
    size_t pos = 0;
    pos = writeVarintField(dataMsg, pos, 1, kPortNodeInfo);
    pos = writeLengthDelimited(dataMsg, pos, 2, user, upos);

    const bool ok = transmitData(dataMsg, pos);
    if (ok) {
        _lastAdvertMs = millis();
        ++_status.advertCount;
    }
    return ok;
}

bool MeshtasticService::transmitData(const uint8_t *plainPayload, size_t len)
{
    if (plainPayload == nullptr || len == 0 || len > kMaxPayloadLen) return false;

    uint8_t packet[kHeaderLen + kMaxPayloadLen];
    const uint32_t toBroadcast = 0xFFFFFFFFu;
    const uint32_t packetId = esp_random();

    // meshtastic/firmware src/mesh/RadioInterface.h PacketHeader - same 16
    // byte layout the decoder in handlePacket() reads.
    packet[0] = static_cast<uint8_t>(toBroadcast);
    packet[1] = static_cast<uint8_t>(toBroadcast >> 8);
    packet[2] = static_cast<uint8_t>(toBroadcast >> 16);
    packet[3] = static_cast<uint8_t>(toBroadcast >> 24);
    packet[4] = static_cast<uint8_t>(_nodeNum);
    packet[5] = static_cast<uint8_t>(_nodeNum >> 8);
    packet[6] = static_cast<uint8_t>(_nodeNum >> 16);
    packet[7] = static_cast<uint8_t>(_nodeNum >> 24);
    packet[8] = static_cast<uint8_t>(packetId);
    packet[9] = static_cast<uint8_t>(packetId >> 8);
    packet[10] = static_cast<uint8_t>(packetId >> 16);
    packet[11] = static_cast<uint8_t>(packetId >> 24);
    packet[12] = (kDefaultHopLimit & 0x07) | ((kDefaultHopLimit & 0x07) << 5);
    packet[13] = _publicChannelHash;
    packet[14] = 0; // next_hop - unset at origin.
    packet[15] = 0; // relay_node - unset at origin.

    memcpy(packet + kHeaderLen, plainPayload, len);

    // Same AES-128-CTR nonce construction as the decrypt path in handlePacket().
    uint8_t nonce[16] = {0};
    nonce[0] = packet[8];
    nonce[1] = packet[9];
    nonce[2] = packet[10];
    nonce[3] = packet[11];
    nonce[8] = packet[4];
    nonce[9] = packet[5];
    nonce[10] = packet[6];
    nonce[11] = packet[7];

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, kDefaultPsk, 128);
    size_t ncOff = 0;
    uint8_t streamBlock[16] = {0};
    mbedtls_aes_crypt_ctr(&ctx, len, &ncOff, nonce, streamBlock, packet + kHeaderLen, packet + kHeaderLen);
    mbedtls_aes_free(&ctx);

    return transmitPacket(packet, kHeaderLen + len);
}

bool MeshtasticService::transmitPacket(const uint8_t *data, size_t len)
{
#if defined(ARDUINO_LILYGO_LORA_SX1262)
    if (!_status.radioReady || data == nullptr || len == 0) return false;

    radio.clearDio1Action();
    const int16_t txState = radio.transmit(data, len);

    // radio.transmit() leaves the chip in a TX-adjacent state - put the
    // Meshtastic RX parameters back before listening again, same pattern
    // MeshService::transmitPacket() uses for MeshCore.
    radio.setFrequency(kFrequencyMhz);
    radio.setBandwidth(kBandwidthKhz);
    radio.setSpreadingFactor(kSpreadingFactor);
    radio.setCodingRate(kCodingRate);
    radio.setSyncWord(kSyncWord);
    radio.setPreambleLength(kPreambleSymbols);
    radio.setCRC(1);
    radio.setDio2AsRfSwitch();
    radio.setDio1Action(packetReceivedThunk);
    const int16_t rxState = radio.startReceive();

    _status.radioError = (txState != RADIOLIB_ERR_NONE) ? txState : rxState;
    _status.radioReady = (rxState == RADIOLIB_ERR_NONE);
    return txState == RADIOLIB_ERR_NONE;
#else
    (void)data;
    (void)len;
    return false;
#endif
}
