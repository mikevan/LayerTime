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

#include "MeshtasticService.h"
#include "esp_mac.h"

#include <Arduino.h>
#include <LilyGoLib.h>
#include <RadioLib.h>
#include <mbedtls/aes.h>
#include <mbedtls/ccm.h>
#include <mbedtls/ecp.h>
#include <mbedtls/bignum.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <Preferences.h>
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
constexpr uint32_t kPortRouting = 5;

// PacketHeader.flags, meshtastic/firmware src/mesh/RadioInterface.h.
constexpr uint8_t kFlagHopLimitMask = 0x07;
constexpr uint8_t kFlagWantAck = 0x08;
constexpr uint8_t kFlagHopStartShift = 5;

// PKC framing, meshtastic/firmware src/mesh/RadioInterface.h and CryptoEngine.cpp.
constexpr size_t kPkcAuthTagLen = 8;
constexpr size_t kPkcExtraNonceLen = 4;
constexpr size_t kPkcOverhead = kPkcAuthTagLen + kPkcExtraNonceLen; // MESHTASTIC_PKC_OVERHEAD = 12
constexpr size_t kPkcNonceLen = 13;
constexpr uint8_t kPkcChannelByte = 0;

// RNG shim for mbedtls' ECP routines: the ESP32 hardware RNG.
int espRandomFill(void *, unsigned char *buf, size_t len)
{
    esp_fill_random(buf, len);
    return 0;
}

// X25519 scalar clamping (RFC 7748). mbedtls clamps when it generates a
// key itself; a key reloaded from NVS goes through here so the same rule
// holds either way.
void clampX25519(uint8_t key[32])
{
    key[0] &= 248;
    key[31] &= 127;
    key[31] |= 64;
}

// scalar * point on Curve25519, both as 32-byte little-endian per RFC 7748.
// Used for both public-key derivation (point = base) and the DH shared
// secret (point = peer's public key).
bool x25519(const uint8_t scalar[32], const uint8_t *pointOrNull, uint8_t out[32])
{
    mbedtls_ecp_group grp;
    mbedtls_ecp_point P, R;
    mbedtls_mpi d;
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&P);
    mbedtls_ecp_point_init(&R);
    mbedtls_mpi_init(&d);

    bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) == 0 &&
              mbedtls_mpi_read_binary_le(&d, scalar, 32) == 0;
    if (ok) {
        if (pointOrNull == nullptr) {
            ok = mbedtls_ecp_copy(&P, &grp.G) == 0;
        } else {
            // Montgomery points are just the x-coordinate, little-endian.
            ok = mbedtls_ecp_point_read_binary(&grp, &P, pointOrNull, 32) == 0;
        }
    }
    if (ok) ok = mbedtls_ecp_mul(&grp, &R, &d, &P, espRandomFill, nullptr) == 0;
    if (ok) {
        size_t olen = 0;
        ok = mbedtls_ecp_point_write_binary(&grp, &R, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, out, 32) == 0 &&
             olen == 32;
    }

    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&R);
    mbedtls_ecp_point_free(&P);
    mbedtls_ecp_group_free(&grp);
    return ok;
}

// CryptoEngine::initNonce(): id LE64 at 0, fromNode LE32 at 8, and if the
// extra nonce is non-zero it overwrites bytes 4-7. Only 13 bytes are fed
// to CCM.
void buildPkcNonce(uint8_t nonce[16], uint32_t fromNum, uint32_t packetId, uint32_t extraNonce)
{
    memset(nonce, 0, 16);
    nonce[0] = static_cast<uint8_t>(packetId);
    nonce[1] = static_cast<uint8_t>(packetId >> 8);
    nonce[2] = static_cast<uint8_t>(packetId >> 16);
    nonce[3] = static_cast<uint8_t>(packetId >> 24);
    nonce[8] = static_cast<uint8_t>(fromNum);
    nonce[9] = static_cast<uint8_t>(fromNum >> 8);
    nonce[10] = static_cast<uint8_t>(fromNum >> 16);
    nonce[11] = static_cast<uint8_t>(fromNum >> 24);
    if (extraNonce != 0) {
        nonce[4] = static_cast<uint8_t>(extraNonce);
        nonce[5] = static_cast<uint8_t>(extraNonce >> 8);
        nonce[6] = static_cast<uint8_t>(extraNonce >> 16);
        nonce[7] = static_cast<uint8_t>(extraNonce >> 24);
    }
}

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

// Wire type 5: fixed32 / sfixed32 / float, little-endian.
size_t writeFixed32Field(uint8_t *buf, size_t pos, uint32_t fieldNum, uint32_t value)
{
    pos = writeTag(buf, pos, fieldNum, 5);
    buf[pos++] = static_cast<uint8_t>(value);
    buf[pos++] = static_cast<uint8_t>(value >> 8);
    buf[pos++] = static_cast<uint8_t>(value >> 16);
    buf[pos++] = static_cast<uint8_t>(value >> 24);
    return pos;
}

size_t writeFloatField(uint8_t *buf, size_t pos, uint32_t fieldNum, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return writeFixed32Field(buf, pos, fieldNum, bits);
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

    loadChannels();

    // Synthesize a NodeNum for ourselves from the chip's factory MAC, same
    // general idea real Meshtastic devices use. We're not a registered
    // device so this is only ever used as our "from" field when we transmit.
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    _nodeNum = (static_cast<uint32_t>(mac[2]) << 24) | (static_cast<uint32_t>(mac[3]) << 16) |
               (static_cast<uint32_t>(mac[4]) << 8) | static_cast<uint32_t>(mac[5]);

    setIdentity(nullptr); // Sensible default until WatchApp supplies the configured name.
    loadOrCreateKeypair();
}

void MeshtasticService::loadOrCreateKeypair()
{
    // The private key has to survive reboots: peers cache our public key
    // from NodeInfo, and a fresh key every boot would make every DM they
    // send us undecryptable until they hear the new one.
    Preferences prefs;
    prefs.begin("mtastic", false);
    const size_t got = prefs.getBytes("x25519priv", _privateKey, sizeof(_privateKey));
    if (got != sizeof(_privateKey)) {
        esp_fill_random(_privateKey, sizeof(_privateKey));
        clampX25519(_privateKey);
        prefs.putBytes("x25519priv", _privateKey, sizeof(_privateKey));
    } else {
        clampX25519(_privateKey);
    }
    prefs.end();
    _keypairReady = x25519(_privateKey, nullptr, _publicKey);
}

// ------------------------------------------------------------ channels

uint8_t MeshtasticService::channelHash(const char *name, const uint8_t *psk, uint8_t pskLen)
{
    // meshtastic/firmware src/mesh/Channels.cpp generateHash():
    // xorHash(name) ^ xorHash(expanded key). LongFast + default key = 8.
    return xorHash(reinterpret_cast<const uint8_t *>(name), strlen(name)) ^ xorHash(psk, pskLen);
}

bool MeshtasticService::parseChannelKey(const char *keyText, uint8_t *pskOut, uint8_t &pskLenOut)
{
    if (keyText == nullptr) return false;
    // Trim surrounding whitespace the keyboard may have left.
    while (*keyText == ' ') ++keyText;
    size_t len = strlen(keyText);
    while (len > 0 && keyText[len - 1] == ' ') --len;
    if (len == 0) return false;

    // Simple keys: Channels::getKey() - index 0 turns encryption off, 1-10
    // are the default key with its last byte bumped by index-1.
    auto applySimple = [&](uint32_t index) -> bool {
        if (index > 10) return false;
        if (index == 0) { pskLenOut = 0; return true; }
        memcpy(pskOut, kDefaultPsk, sizeof(kDefaultPsk));
        pskOut[sizeof(kDefaultPsk) - 1] = static_cast<uint8_t>(pskOut[sizeof(kDefaultPsk) - 1] + (index - 1));
        pskLenOut = sizeof(kDefaultPsk);
        return true;
    };

    bool digits = true;
    for (size_t i = 0; i < len; ++i) if (keyText[i] < '0' || keyText[i] > '9') { digits = false; break; }
    if (digits && len <= 2) return applySimple(static_cast<uint32_t>(atoi(keyText)));

    uint8_t decoded[48];
    size_t decodedLen = 0;
    if (mbedtls_base64_decode(decoded, sizeof(decoded), &decodedLen,
                              reinterpret_cast<const unsigned char *>(keyText), len) != 0) {
        return false;
    }
    if (decodedLen == 1) return applySimple(decoded[0]);
    if (decodedLen == 16 || decodedLen == 32) {
        memcpy(pskOut, decoded, decodedLen);
        pskLenOut = static_cast<uint8_t>(decodedLen);
        return true;
    }
    return false;
}

void MeshtasticService::channelKeyText(const MeshtasticChannel &channel, char *out, size_t outSize)
{
    if (out == nullptr || outSize == 0) return;
    out[0] = '\0';
    if (channel.pskLen == 0) { snprintf(out, outSize, "0"); return; }
    size_t written = 0;
    mbedtls_base64_encode(reinterpret_cast<unsigned char *>(out), outSize, &written, channel.psk, channel.pskLen);
}

uint8_t MeshtasticService::freeChannelSlot() const
{
    for (uint8_t i = 1; i < MeshtasticStatus::kMaxChannels; ++i) {
        if (!_status.channels[i].used) return i;
    }
    return MeshtasticStatus::kMaxChannels;
}

void MeshtasticService::loadChannels()
{
    // Slot 0: the primary channel every stock device boots on. Fixed.
    MeshtasticChannel &primary = _status.channels[0];
    primary.used = true;
    snprintf(primary.name, sizeof(primary.name), "%s", kDefaultChannelName);
    memcpy(primary.psk, kDefaultPsk, sizeof(kDefaultPsk));
    primary.pskLen = sizeof(kDefaultPsk);
    primary.hash = channelHash(primary.name, primary.psk, primary.pskLen);

    // Slots 1-7 from NVS: "ch<N>" = name[12] || pskLen || psk[32].
    Preferences prefs;
    prefs.begin("mtastic", true);
    for (uint8_t i = 1; i < MeshtasticStatus::kMaxChannels; ++i) {
        MeshtasticChannel &ch = _status.channels[i];
        ch = MeshtasticChannel{};
        char key[8];
        snprintf(key, sizeof(key), "ch%u", static_cast<unsigned>(i));
        uint8_t blob[12 + 1 + 32];
        if (prefs.getBytes(key, blob, sizeof(blob)) != sizeof(blob)) continue;
        memcpy(ch.name, blob, sizeof(ch.name));
        ch.name[sizeof(ch.name) - 1] = '\0';
        ch.pskLen = blob[12];
        if (ch.pskLen != 0 && ch.pskLen != 16 && ch.pskLen != 32) continue;
        memcpy(ch.psk, blob + 13, sizeof(ch.psk));
        ch.hash = channelHash(ch.name, ch.psk, ch.pskLen);
        ch.used = ch.name[0] != '\0';
    }
    prefs.end();
}

void MeshtasticService::saveChannel(uint8_t index)
{
    if (index == 0 || index >= MeshtasticStatus::kMaxChannels) return;
    const MeshtasticChannel &ch = _status.channels[index];
    char key[8];
    snprintf(key, sizeof(key), "ch%u", static_cast<unsigned>(index));
    Preferences prefs;
    prefs.begin("mtastic", false);
    if (!ch.used) {
        prefs.remove(key);
    } else {
        uint8_t blob[12 + 1 + 32] = {0};
        memcpy(blob, ch.name, sizeof(ch.name));
        blob[12] = ch.pskLen;
        memcpy(blob + 13, ch.psk, sizeof(ch.psk));
        prefs.putBytes(key, blob, sizeof(blob));
    }
    prefs.end();
}

bool MeshtasticService::setChannel(uint8_t index, const char *name, const char *keyText)
{
    if (index == 0 || index >= MeshtasticStatus::kMaxChannels) return false;
    if (name == nullptr) return false;
    while (*name == ' ') ++name;
    size_t nameLen = strlen(name);
    while (nameLen > 0 && name[nameLen - 1] == ' ') --nameLen;
    if (nameLen == 0 || nameLen > 11) return false;

    MeshtasticChannel ch;
    if (!parseChannelKey(keyText, ch.psk, ch.pskLen)) return false;
    memcpy(ch.name, name, nameLen);
    ch.name[nameLen] = '\0';
    ch.hash = channelHash(ch.name, ch.psk, ch.pskLen);
    ch.used = true;
    _status.channels[index] = ch;
    saveChannel(index);
    return true;
}

bool MeshtasticService::removeChannel(uint8_t index)
{
    if (index == 0 || index >= MeshtasticStatus::kMaxChannels) return false;
    _status.channels[index] = MeshtasticChannel{};
    saveChannel(index);
    return true;
}

void MeshtasticService::cryptChannel(uint8_t channelIndex, uint32_t packetId, uint32_t fromNum,
                                     uint8_t *data, size_t len) const
{
    if (channelIndex >= MeshtasticStatus::kMaxChannels) return;
    const MeshtasticChannel &ch = _status.channels[channelIndex];
    if (ch.pskLen == 0) return; // open channel: payload is plaintext on air

    // meshtastic/firmware src/mesh/CryptoEngine.cpp initNonce(): 16-byte
    // nonce = packetId as zero-extended LE64 (bytes 0-7) || fromNode as LE32
    // (bytes 8-11) || zero (bytes 12-15). CTR is symmetric, so this both
    // encrypts and decrypts. Key length picks AES-128 or AES-256.
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
    mbedtls_aes_setkey_enc(&ctx, ch.psk, ch.pskLen == 32 ? 256 : 128); // CTR always uses the encrypt schedule.
    size_t ncOff = 0;
    uint8_t streamBlock[16] = {0};
    mbedtls_aes_crypt_ctr(&ctx, len, &ncOff, nonce, streamBlock, data, data);
    mbedtls_aes_free(&ctx);
}

bool MeshtasticService::deriveSharedKey(const uint8_t peerPublic[32], uint8_t keyOut[32]) const
{
    if (!_keypairReady) return false;
    uint8_t shared[32];
    if (!x25519(_privateKey, peerPublic, shared)) return false;
    // CryptoEngine::hash(): the raw DH output is SHA-256'd into the AES key.
    const int rc = mbedtls_sha256(shared, sizeof(shared), keyOut, 0);
    memset(shared, 0, sizeof(shared));
    return rc == 0;
}

bool MeshtasticService::decryptPkc(uint32_t fromNum, uint32_t packetId, const uint8_t *peerPublic,
                                   const uint8_t *in, size_t inLen, uint8_t *out, size_t &outLen) const
{
    if (inLen <= kPkcOverhead) return false;
    const size_t cipherLen = inLen - kPkcOverhead;
    const uint8_t *tag = in + cipherLen;
    const uint8_t *extra = tag + kPkcAuthTagLen;
    const uint32_t extraNonce = readLe32(extra);

    uint8_t key[32];
    if (!deriveSharedKey(peerPublic, key)) return false;
    uint8_t nonce[16];
    buildPkcNonce(nonce, fromNum, packetId, extraNonce);

    mbedtls_ccm_context ctx;
    mbedtls_ccm_init(&ctx);
    bool ok = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
              mbedtls_ccm_auth_decrypt(&ctx, cipherLen, nonce, kPkcNonceLen, nullptr, 0,
                                       in, out, tag, kPkcAuthTagLen) == 0;
    mbedtls_ccm_free(&ctx);
    memset(key, 0, sizeof(key));
    outLen = ok ? cipherLen : 0;
    return ok;
}

bool MeshtasticService::encryptPkc(uint32_t packetId, const uint8_t *peerPublic, const uint8_t *in,
                                   size_t inLen, uint8_t *out, size_t &outLen) const
{
    if (inLen + kPkcOverhead > kMaxPayloadLen) return false;
    uint32_t extraNonce = 0;
    do { extraNonce = esp_random(); } while (extraNonce == 0); // 0 means 'none' in initNonce()

    uint8_t key[32];
    if (!deriveSharedKey(peerPublic, key)) return false;
    uint8_t nonce[16];
    buildPkcNonce(nonce, _nodeNum, packetId, extraNonce);

    uint8_t *tag = out + inLen;
    mbedtls_ccm_context ctx;
    mbedtls_ccm_init(&ctx);
    const bool ok = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
                    mbedtls_ccm_encrypt_and_tag(&ctx, inLen, nonce, kPkcNonceLen, nullptr, 0,
                                                in, out, tag, kPkcAuthTagLen) == 0;
    mbedtls_ccm_free(&ctx);
    memset(key, 0, sizeof(key));
    if (!ok) return false;

    uint8_t *extra = tag + kPkcAuthTagLen;
    extra[0] = static_cast<uint8_t>(extraNonce);
    extra[1] = static_cast<uint8_t>(extraNonce >> 8);
    extra[2] = static_cast<uint8_t>(extraNonce >> 16);
    extra[3] = static_cast<uint8_t>(extraNonce >> 24);
    outLen = inLen + kPkcOverhead;
    return true;
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
    pollPeriodicBroadcasts(nowMs);
    pollRetransmits(nowMs);

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
    const uint32_t toNum = readLe32(raw + 0);
    const uint32_t fromNum = readLe32(raw + 4);
    const uint32_t packetId = readLe32(raw + 8);
    const uint8_t flags = raw[12];
    const uint8_t channelHash = raw[13];

    // Our own packet, rebroadcast by a neighbour. Never treat it as a
    // received message - but it is the implicit ack for a broadcast, and
    // evidence a DM is on its way. Previously this fell straight through
    // and every one of our sends came back as a duplicate 'received' entry.
    if (fromNum == _nodeNum) {
        noteRebroadcast(packetId);
        return;
    }

    // Only traffic for us: the broadcast address, or our own node number.
    // A DM between two other nodes is not ours to decode.
    if (toNum != kMeshtasticBroadcast && toNum != _nodeNum) return;

    // Each neighbour rebroadcasts what it hears, so one packet arrives
    // several times.
    if (seenPacket(fromNum, packetId)) return;

    const uint8_t hopLimit = flags & kFlagHopLimitMask;
    const uint8_t hopStart = (flags >> kFlagHopStartShift) & 0x07;
    const bool wantAck = (flags & kFlagWantAck) != 0;

    // Router::perhapsDecode(): channel byte 0, addressed to us, not
    // broadcast, and we know the sender's public key = a PKC direct
    // message. Decrypt with the shared key instead of the channel PSK.
    if (channelHash == kPkcChannelByte && toNum == _nodeNum) {
        const MeshtasticNode *peer = nullptr;
        for (const MeshtasticNode &n : _status.nodes) {
            if (n.used && n.num == fromNum && n.hasPublicKey) { peer = &n; break; }
        }
        if (peer == nullptr) return; // no key for them yet - wait for their NodeInfo
        uint8_t plain[kMaxPayloadLen];
        size_t plainLen = 0;
        if (!decryptPkc(fromNum, packetId, peer->publicKey, raw + kHeaderLen, len - kHeaderLen, plain, plainLen)) {
            return; // auth tag failed: wrong key, or not for us after all
        }
        _pkcInbound = true;
        _rxChannel = 0;
        handleData(fromNum, toNum, packetId, wantAck, hopLimit, hopStart, rssi, snr, plain, plainLen);
        _pkcInbound = false;
        return;
    }

    // Channels::getChannelIndexByHash(): the header carries only the 8-bit
    // hash, so the first slot whose hash matches is the one we decode with
    // (same as the firmware - colliding hashes are a known limitation).
    uint8_t channelIndex = MeshtasticStatus::kMaxChannels;
    for (uint8_t i = 0; i < MeshtasticStatus::kMaxChannels; ++i) {
        if (_status.channels[i].used && _status.channels[i].hash == channelHash) { channelIndex = i; break; }
    }
    if (channelIndex == MeshtasticStatus::kMaxChannels) {
        return; // a channel we don't have the key for
    }

    const size_t payloadLen = len - kHeaderLen;
    if (payloadLen == 0 || payloadLen > kMaxPayloadLen) return;

    uint8_t decrypted[kMaxPayloadLen];
    memcpy(decrypted, raw + kHeaderLen, payloadLen);
    cryptChannel(channelIndex, packetId, fromNum, decrypted, payloadLen);

    // Remember which channel this node talks on, so a DM back to it (when
    // we hold no key for it) uses the channel it can actually hear.
    for (MeshtasticNode &n : _status.nodes) {
        if (n.used && n.num == fromNum) { n.channel = channelIndex; break; }
    }

    _rxChannel = channelIndex;
    handleData(fromNum, toNum, packetId, wantAck, hopLimit, hopStart, rssi, snr, decrypted, payloadLen);
}

void MeshtasticService::handleData(uint32_t fromNum, uint32_t toNum, uint32_t packetId, bool wantAck,
                                    uint8_t hopLimit, uint8_t hopStart, float rssi, float snr,
                                    const uint8_t *decrypted, size_t len)
{
    // meshtastic/protobufs mesh.proto Data: 1=portnum(varint) 2=payload(bytes)
    // 3=want_response 4=dest(fixed32) 5=source(fixed32) 6=request_id(fixed32)
    // 7=reply_id(fixed32) 8=emoji(fixed32). request_id is how a ROUTING_APP
    // ack names the packet it acknowledges.
    size_t offset = 0;
    uint32_t portnum = 0;
    uint32_t requestId = 0;
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
        } else if (fieldNum == 6 && wireType == 5) {
            if (offset + 4 > len) break;
            requestId = readLe32(decrypted + offset);
            offset += 4;
        } else {
            if (!skipField(decrypted, len, offset, wireType)) break;
        }
    }

    // A Routing ack carries an EMPTY payload (error_reason NONE=0 is the
    // proto default and is elided), so 'no payload' is legitimate there and
    // must not be treated as a decode failure.
    if (portnum == kPortRouting) {
        ++_status.decodedCount;
        handleRouting(fromNum, requestId, payload, payloadLen);
        return;
    }

    if (payload == nullptr) {
        // Failed to decode as a Data message at all - almost always means the
        // decrypt key/nonce didn't line up (e.g. a non-default channel that
        // happened to share our channel hash byte). Not worth surfacing as
        // an error; just drop it.
        return;
    }

    ++_status.decodedCount;

    // A DM to us that asked for an ack gets one, regardless of port -
    // otherwise the sender's message sits at Pending and eventually fails
    // even though we got it. Broadcasts are never acked (that would flood).
    if (wantAck && toNum == _nodeNum) {
        const uint8_t *peerKey = nullptr;
        if (_pkcInbound) {
            for (const MeshtasticNode &n : _status.nodes) {
                if (n.used && n.num == fromNum && n.hasPublicKey) { peerKey = n.publicKey; break; }
            }
        }
        sendAck(fromNum, packetId, _rxChannel, peerKey);
    }

    switch (portnum) {
        case kPortTextMessage:
            handleTextMessage(fromNum, toNum, packetId, hopLimit, rssi, snr, payload, payloadLen);
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
        } else if (fieldNum == 8 && wireType == 2) {
            // User.public_key - a 32-byte Curve25519 key when present.
            uint64_t l;
            if (!readVarint(payload, len, offset, l)) break;
            if (offset + l > len) break;
            if (l == sizeof(node->publicKey)) {
                memcpy(node->publicKey, payload + offset, sizeof(node->publicKey));
                node->hasPublicKey = true;
            }
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

void MeshtasticService::handleTextMessage(uint32_t fromNum, uint32_t toNum, uint32_t packetId,
                                           uint8_t hopLimit, float rssi, float snr,
                                           const uint8_t *payload, size_t len)
{
    // TEXT_MESSAGE_APP payload is raw UTF-8 bytes, not further protobuf-wrapped.
    MeshtasticMessage *msg = appendMessage();
    msg->fromNum = fromNum;
    msg->toNum = toNum;
    msg->packetId = packetId;
    msg->isOurs = false;
    msg->pkiEncrypted = _pkcInbound;
    msg->channel = _rxChannel;
    msg->rssi = rssi;
    msg->snr = snr;
    msg->hopLimit = hopLimit;
    msg->receivedMs = millis();

    const size_t n = len < sizeof(msg->text) - 1 ? len : sizeof(msg->text) - 1;
    memcpy(msg->text, payload, n);
    msg->text[n] = '\0';
}

void MeshtasticService::handleRouting(uint32_t fromNum, uint32_t requestId, const uint8_t *payload, size_t len)
{
    // Routing (mesh.proto): 3=error_reason(varint). Absent means NONE=0, an
    // ack. Anything else is a nak - NO_ROUTE, GOT_NAK, TIMEOUT, ...
    uint32_t errorReason = 0;
    size_t offset = 0;
    while (payload != nullptr && offset < len) {
        uint64_t tag;
        if (!readVarint(payload, len, offset, tag)) break;
        const uint32_t fieldNum = static_cast<uint32_t>(tag >> 3);
        const uint8_t wireType = static_cast<uint8_t>(tag & 0x07);
        if (fieldNum == 3 && wireType == 0) {
            uint64_t v;
            if (!readVarint(payload, len, offset, v)) break;
            errorReason = static_cast<uint32_t>(v);
        } else if (!skipField(payload, len, offset, wireType)) {
            break;
        }
    }

    MeshtasticMessage *msg = findOurMessage(requestId);
    if (msg != nullptr) {
        msg->delivery = (errorReason == 0) ? MeshtasticDelivery::Acked : MeshtasticDelivery::Failed;
    }
    for (PendingTx &tx : _pending) {
        if (tx.used && tx.packetId == requestId) tx.used = false;
    }
    (void)fromNum;
}

MeshtasticMessage *MeshtasticService::appendMessage()
{
    MeshtasticMessage &msg = _status.messages[_messageWrite];
    msg = MeshtasticMessage{};
    msg.used = true;
    _messageWrite = (_messageWrite + 1) % MeshtasticStatus::kMaxMessages;
    if (_status.messageCount < MeshtasticStatus::kMaxMessages) ++_status.messageCount;
    return &msg;
}

MeshtasticMessage *MeshtasticService::findOurMessage(uint32_t packetId)
{
    if (packetId == 0) return nullptr;
    for (MeshtasticMessage &m : _status.messages) {
        if (m.used && m.isOurs && m.packetId == packetId) return &m;
    }
    return nullptr;
}

bool MeshtasticService::seenPacket(uint32_t fromNum, uint32_t packetId)
{
    for (const SeenPacket &p : _seen) {
        if (p.packetId == packetId && p.fromNum == fromNum) return true;
    }
    _seen[_seenWrite] = {fromNum, packetId};
    _seenWrite = static_cast<uint8_t>((_seenWrite + 1) % (sizeof(_seen) / sizeof(_seen[0])));
    return false;
}

void MeshtasticService::noteRebroadcast(uint32_t packetId)
{
    MeshtasticMessage *msg = findOurMessage(packetId);
    if (msg == nullptr) return;
    if (msg->toNum == kMeshtasticBroadcast) {
        // Broadcasts get no explicit ack - hearing a neighbour repeat it is
        // as delivered as it gets (Meshtastic's own definition).
        msg->delivery = MeshtasticDelivery::Acked;
        for (PendingTx &tx : _pending) {
            if (tx.used && tx.packetId == packetId) tx.used = false;
        }
    } else if (msg->delivery == MeshtasticDelivery::Pending) {
        msg->delivery = MeshtasticDelivery::Relayed;
    }
}

bool MeshtasticService::sendAck(uint32_t toNum, uint32_t requestId, uint8_t channelIndex,
                                const uint8_t *peerPublicKey)
{
    // Data{portnum=ROUTING_APP, request_id=<their id>} with an empty Routing
    // payload - error_reason NONE is the default and is not encoded.
    uint8_t dataMsg[16];
    size_t pos = 0;
    pos = writeVarintField(dataMsg, pos, 1, kPortRouting);
    pos = writeFixed32Field(dataMsg, pos, 6, requestId);
    uint32_t id = 0;
    return transmitData(dataMsg, pos, toNum, false, id, false, peerPublicKey, channelIndex);
}

void MeshtasticService::pollRetransmits(uint32_t nowMs)
{
    for (PendingTx &tx : _pending) {
        if (!tx.used || nowMs - tx.lastSentMs < kRetransmitMs) continue;
        if (tx.attempts >= kMaxAttempts) {
            MeshtasticMessage *msg = findOurMessage(tx.packetId);
            if (msg != nullptr && msg->delivery != MeshtasticDelivery::Acked) {
                msg->delivery = MeshtasticDelivery::Failed;
            }
            tx.used = false;
            continue;
        }
        // Byte-identical resend, same id: the recipient's dedupe drops the
        // duplicate but its ack still gets through.
        if (transmitPacket(tx.packet, tx.len)) {
            ++tx.attempts;
            tx.lastSentMs = nowMs;
        }
    }
}

void MeshtasticService::pollPeriodicBroadcasts(uint32_t nowMs)
{
    if (!_status.advertisingEnabled) return;
    if (_ownPositionValid && (_lastPositionMs == 0 || nowMs - _lastPositionMs >= kPositionIntervalMs)) {
        sendPositionNow();
    }
    if (_lastTelemetryMs == 0 || nowMs - _lastTelemetryMs >= kTelemetryIntervalMs) {
        sendTelemetryNow();
    }
}

void MeshtasticService::setOwnPosition(bool valid, double latitude, double longitude, int32_t altitudeM)
{
    _ownPositionValid = valid;
    _ownLatitude = latitude;
    _ownLongitude = longitude;
    _ownAltitudeM = altitudeM;
}

bool MeshtasticService::ownPosition(double &latitude, double &longitude) const
{
    if (!_ownPositionValid) return false;
    latitude = _ownLatitude;
    longitude = _ownLongitude;
    return true;
}

void MeshtasticService::setOwnBattery(uint8_t percent)
{
    _ownBatteryPercent = percent;
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
    return sendChannelMessage(0, text);
}

bool MeshtasticService::sendChannelMessage(uint8_t channelIndex, const char *text)
{
    if (channelIndex >= MeshtasticStatus::kMaxChannels || !_status.channels[channelIndex].used) return false;
    return sendText(kMeshtasticBroadcast, channelIndex, text);
}

bool MeshtasticService::sendDirectMessage(uint32_t toNum, const char *text)
{
    if (toNum == kMeshtasticBroadcast) return sendChannelMessage(0, text);
    // Without a key for the peer the DM rides the channel we last heard it
    // on (NodeDB keeps the same per-node channel for this reason).
    uint8_t channelIndex = 0;
    for (const MeshtasticNode &n : _status.nodes) {
        if (n.used && n.num == toNum) { channelIndex = n.channel; break; }
    }
    if (channelIndex >= MeshtasticStatus::kMaxChannels || !_status.channels[channelIndex].used) channelIndex = 0;
    return sendText(toNum, channelIndex, text);
}

bool MeshtasticService::sendText(uint32_t toNum, uint8_t channelIndex, const char *text)
{
    if (text == nullptr || text[0] == '\0' || !_status.radioReady) return false;

    const size_t textLen = strlen(text);
    if (textLen > 200) return false; // Stay well under kMaxPayloadLen once the Data wrapper is added.

    // Data: 1=portnum(varint), 2=payload(bytes). meshtastic/protobufs mesh.proto.
    uint8_t dataMsg[220];
    size_t pos = 0;
    pos = writeVarintField(dataMsg, pos, 1, kPortTextMessage);
    pos = writeLengthDelimited(dataMsg, pos, 2, reinterpret_cast<const uint8_t *>(text), textLen);

    // Every text send is reliable: a DM asks the recipient for an explicit
    // ack, a broadcast relies on hearing a neighbour rebroadcast it. Both
    // are tracked in _pending and retried.
    const bool isBroadcast = (toNum == kMeshtasticBroadcast);
    // A DM to a node that has published a public key goes end-to-end
    // encrypted with it. Modern (>= 2.5) nodes reject PSK-encrypted DMs
    // from a peer they hold a key for, so this is not optional for them.
    const uint8_t *peerKey = nullptr;
    if (!isBroadcast) {
        for (const MeshtasticNode &n : _status.nodes) {
            if (n.used && n.num == toNum && n.hasPublicKey) { peerKey = n.publicKey; break; }
        }
    }
    uint32_t packetId = 0;
    const bool ok = transmitData(dataMsg, pos, toNum, !isBroadcast, packetId, true, peerKey, channelIndex);
    if (ok) {
        MeshtasticMessage *msg = appendMessage();
        msg->pkiEncrypted = (peerKey != nullptr);
        msg->channel = (peerKey != nullptr) ? 0 : channelIndex;
        msg->fromNum = _nodeNum;
        msg->toNum = toNum;
        msg->packetId = packetId;
        msg->isOurs = true;
        msg->delivery = MeshtasticDelivery::Pending;
        msg->hopLimit = kDefaultHopLimit;
        msg->receivedMs = millis();
        const size_t n = textLen < sizeof(msg->text) - 1 ? textLen : sizeof(msg->text) - 1;
        memcpy(msg->text, text, n);
        msg->text[n] = '\0';
    }
    return ok;
}

bool MeshtasticService::sendPositionNow()
{
    if (!_status.radioReady || !_ownPositionValid) return false;

    // Position (mesh.proto): 1=latitude_i(sfixed32, 1e-7 deg), 2=longitude_i,
    // 3=altitude(int32, metres), 4=time(fixed32, unix). No RTC-derived unix
    // time is plumbed in here yet, so time is omitted - Meshtastic treats a
    // missing time as 'unknown' rather than rejecting the position.
    uint8_t position[40];
    size_t ppos = 0;
    const int32_t latI = static_cast<int32_t>(_ownLatitude * 1e7);
    const int32_t lonI = static_cast<int32_t>(_ownLongitude * 1e7);
    ppos = writeFixed32Field(position, ppos, 1, static_cast<uint32_t>(latI));
    ppos = writeFixed32Field(position, ppos, 2, static_cast<uint32_t>(lonI));
    // int32 on the wire is a varint of the value sign-extended to 64 bits.
    ppos = writeVarintField(position, ppos, 3, static_cast<uint64_t>(static_cast<int64_t>(_ownAltitudeM)));

    uint8_t dataMsg[64];
    size_t pos = 0;
    pos = writeVarintField(dataMsg, pos, 1, kPortPosition);
    pos = writeLengthDelimited(dataMsg, pos, 2, position, ppos);

    uint32_t id = 0;
    const bool ok = transmitData(dataMsg, pos, kMeshtasticBroadcast, false, id);
    if (ok) _lastPositionMs = millis();
    return ok;
}

bool MeshtasticService::sendTelemetryNow()
{
    if (!_status.radioReady) return false;

    // Telemetry (telemetry.proto): 2=device_metrics(DeviceMetrics), where
    // DeviceMetrics: 1=battery_level(uint32) 2=voltage(float)
    // 3=channel_utilization(float) 4=air_util_tx(float). Only the battery
    // is known here; the others are omitted rather than reported as zero.
    uint8_t metrics[16];
    size_t mpos = 0;
    mpos = writeVarintField(metrics, mpos, 1, _ownBatteryPercent);

    uint8_t telemetry[24];
    size_t tpos = 0;
    tpos = writeLengthDelimited(telemetry, tpos, 2, metrics, mpos);

    uint8_t dataMsg[40];
    size_t pos = 0;
    pos = writeVarintField(dataMsg, pos, 1, kPortTelemetry);
    pos = writeLengthDelimited(dataMsg, pos, 2, telemetry, tpos);

    uint32_t id = 0;
    const bool ok = transmitData(dataMsg, pos, kMeshtasticBroadcast, false, id);
    if (ok) _lastTelemetryMs = millis();
    return ok;
}

bool MeshtasticService::sendNodeInfoNow()
{
    if (!_status.radioReady) return false;

    // User: 2=long_name(string), 3=short_name(string). meshtastic/protobufs mesh.proto.
    uint8_t user[120];
    size_t upos = 0;
    upos = writeStringField(user, upos, 2, _status.longName);
    upos = writeStringField(user, upos, 3, _shortName);
    // User.public_key (field 8): what lets other nodes DM us end-to-end.
    if (_keypairReady) upos = writeLengthDelimited(user, upos, 8, _publicKey, sizeof(_publicKey));

    // Data: 1=portnum(varint), 2=payload(bytes), wrapping the User message above.
    uint8_t dataMsg[120];
    size_t pos = 0;
    pos = writeVarintField(dataMsg, pos, 1, kPortNodeInfo);
    pos = writeLengthDelimited(dataMsg, pos, 2, user, upos);

    uint32_t id = 0;
    const bool ok = transmitData(dataMsg, pos, kMeshtasticBroadcast, false, id);
    if (ok) {
        _lastAdvertMs = millis();
        ++_status.advertCount;
    }
    return ok;
}

bool MeshtasticService::transmitData(const uint8_t *plainPayload, size_t len, uint32_t toNum,
                                     bool wantAck, uint32_t &packetIdOut, bool reliable,
                                     const uint8_t *peerPublicKey, uint8_t channelIndex)
{
    if (plainPayload == nullptr || len == 0 || len > kMaxPayloadLen) return false;
    if (channelIndex >= MeshtasticStatus::kMaxChannels || !_status.channels[channelIndex].used) return false;
    const bool usePkc = (peerPublicKey != nullptr);

    uint8_t packet[kHeaderLen + kMaxPayloadLen];
    const uint32_t toBroadcast = toNum;
    const uint32_t packetId = esp_random();
    packetIdOut = packetId;

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
    packet[12] = static_cast<uint8_t>((kDefaultHopLimit & kFlagHopLimitMask) |
                                      ((kDefaultHopLimit & 0x07) << kFlagHopStartShift) |
                                      (wantAck ? kFlagWantAck : 0));
    packet[13] = usePkc ? kPkcChannelByte : _status.channels[channelIndex].hash;
    packet[14] = 0; // next_hop - unset at origin.
    packet[15] = 0; // relay_node - unset at origin.

    size_t bodyLen = len;
    if (usePkc) {
        if (!encryptPkc(packetId, peerPublicKey, plainPayload, len, packet + kHeaderLen, bodyLen)) return false;
        const bool sent = transmitPacket(packet, kHeaderLen + bodyLen);
        if (sent && reliable) {
            for (PendingTx &tx : _pending) {
                if (tx.used) continue;
                tx.used = true;
                tx.packetId = packetId;
                tx.toNum = toNum;
                tx.attempts = 1;
                tx.lastSentMs = millis();
                tx.len = kHeaderLen + bodyLen;
                memcpy(tx.packet, packet, tx.len);
                break;
            }
        }
        return sent;
    }

    memcpy(packet + kHeaderLen, plainPayload, len);
    cryptChannel(channelIndex, packetId, _nodeNum, packet + kHeaderLen, len);

    const bool ok = transmitPacket(packet, kHeaderLen + len);

    if (ok && reliable) {
        for (PendingTx &tx : _pending) {
            if (tx.used) continue;
            tx.used = true;
            tx.packetId = packetId;
            tx.toNum = toNum;
            tx.attempts = 1;
            tx.lastSentMs = millis();
            tx.len = kHeaderLen + len;
            memcpy(tx.packet, packet, tx.len);
            break;
        }
    }
    return ok;
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
