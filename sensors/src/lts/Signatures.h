// LayerTime-Sensors - passive wireless threat detectors for small radios.
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

// Signature tables and the pure matchers that read them. Everything here
// works on plain values and bytes; capturing frames and advertisements is
// the caller's job.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "Detectors.h"

namespace lts {

// ---- BLE signature tables ----------------------------------------------
// Matching is done against PARSED advertisement fields - the manufacturer
// record's company ID, and the 16-bit service UUIDs - never by searching
// the raw payload for byte pairs. A two-byte pattern scanned across a
// 31-byte advertisement matches by coincidence often enough to make a
// wrist alert useless, which is what the previous implementation did.

inline constexpr uint16_t kAppleCompanyId = 0x004C;
inline constexpr uint16_t kXuntongCompanyId = 0x09C8;  // Flock's BLE radio supplier

// Apple Find My offline-finding subtypes: 0x12 near owner, 0x1E separated.
// SquachWatch-CYD additionally matches 0x07 ("proximity pairing"), which
// catches a tag earlier but also fires on AirPods and other Apple
// accessories. Off here on purpose: this drives a buzz on the wrist, and a
// detector that trips on the owner's own earbuds gets ignored. Flip to
// true to trade precision for earlier warning.
inline constexpr bool kMatchProximityPairing = false;

struct BleUuidSignature {
    uint16_t uuid;
    DetectorId detector;
    const char *label;
    Confidence confidence;
};

inline constexpr BleUuidSignature kBleUuidSignatures[] = {
    // Bluetooth SIG assigned, exclusive to one product line.
    {0xFD5F, DetectorId::Meta, "Meta Ray-Ban glasses", Confidence::High},
    {0xFEED, DetectorId::Tile, "Tile tracker", Confidence::High},
    {0xFEEC, DetectorId::Tile, "Tile tracker", Confidence::High},
    {0xFD5A, DetectorId::SamsungTag, "Samsung SmartTag", Confidence::High},
    // Flipper Zero case colours. Carried over from the previous byte
    // patterns {0x81,0x30}/{0x82,0x30}/{0x83,0x30}, read as little-endian
    // 16-bit UUIDs. UNVERIFIED against hardware - if Flipper detection
    // regresses, this reading is the first thing to re-check.
    {0x3081, DetectorId::Flipper, "Flipper Zero", Confidence::Medium},
    {0x3082, DetectorId::Flipper, "Flipper Zero", Confidence::Medium},
    {0x3083, DetectorId::Flipper, "Flipper Zero", Confidence::Medium},
    // 0xFEAA is Google's general Eddystone UUID - shared with retail and
    // asset beacons that are not trackers, so a match means "Google
    // beacon-class device", not "Find My Device tracker".
    {0xFEAA, DetectorId::GoogleTag, "Google Find My Device", Confidence::Medium},
    // Meta/Facebook service UUIDs carried over from the old byte patterns.
    // Unsourced, hence Low - the three remaining old patterns ({0xAB,0x01},
    // {0x8E,0x05}, {0x53,0x0D}) were dropped outright: two bytes each, no
    // provenance, and short enough to match noise constantly.
    {0xFEB7, DetectorId::Meta, "Meta service", Confidence::Low},
    {0xFEB8, DetectorId::Meta, "Meta service", Confidence::Low},
};

bool isFindMyBeacon(const uint8_t *mfg, size_t length);

bool allDigits(const std::string &value);

// Flock's BLE radio is a generic XUNTONG module, so the company ID alone is
// not enough - it is gated on a plausible device name. Kept from the
// previous implementation, which is stricter than SquachWatch's bare
// company-ID match and worth keeping.
bool isFlockName(const std::string &name);

void formatMac(char *out, size_t outSize, const uint8_t *mac);

uint16_t hashSsid(const uint8_t *ssid, size_t length);

bool isPineappleOui(const uint8_t *mac, bool openNetwork);

struct OuiSignature {
    uint8_t oui[3];
    DetectorId detector;
    Confidence confidence;
    const char *label;
};

// Flock: LayerTime's original 8 prefixes plus SquachWatch-CYD's set, which
// were COMPLETELY DISJOINT from ours - zero overlap across 8 and 29.
//
// The split matters more than the merge. Over half of his entries are
// generic Espressif MA-L blocks, and his own source labels them that way
// (Flock-ESP32 / Flock-ESP-S3 / Flock-ESP-C6) even though his docs grade
// the whole set "High confidence". Those match any ESP32 dev board, ESP
// smart plug or hobby project in range - including, in principle, the
// device running this code. They are kept because coverage is coverage, but
// graded Low, so a consumer can log them without ever raising an alert.
//
// Deliberately NOT included: 82:6B:F2, which SquachWatch lists as
// "Flock-DeFlk". Its first octet has the locally-administered bit set
// (0x82 = 1000 0010), so it is a randomised address someone observed once,
// not a registered vendor prefix at all.
inline constexpr OuiSignature kOuiSignatures[] = {
    // --- Flock, registered prefixes (LayerTime originals) ---
    {{0x58,0x8E,0x81}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0xEC,0x1B,0xBD}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x90,0x35,0xEA}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x04,0x0D,0x84}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0xF0,0x82,0xC0}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x1C,0x34,0xF1}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x38,0x5B,0x44}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x94,0x34,0x69}, DetectorId::Flock, Confidence::High, "Flock"},
    // --- Flock, registered prefixes (from SquachWatch-CYD) ---
    {{0xB4,0x1E,0x52}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x24,0xB2,0xB9}, DetectorId::Flock, Confidence::High, "Flock Liteon"},
    {{0xD0,0x39,0x57}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x00,0xF4,0x8D}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x14,0x5A,0xFC}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x80,0x30,0x49}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0xE0,0x0A,0xF6}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x70,0xC9,0x4E}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x3C,0x91,0x80}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0xD8,0xF3,0xBC}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0xB8,0x35,0x32}, DetectorId::Flock, Confidence::High, "Flock"},
    {{0x00,0xA0,0xD8}, DetectorId::Flock, Confidence::High, "Flock Sierra"},
    // --- Axon / Taser body cameras ---
    {{0x00,0x25,0xDF}, DetectorId::Axon, Confidence::High, "Axon (Taser)"},
    {{0xE4,0x05,0x40}, DetectorId::Axon, Confidence::High, "Axon body cam"},
    {{0x28,0x24,0xFF}, DetectorId::Axon, Confidence::High, "Axon Signal"},
    // --- Flock, generic Espressif blocks: LOW, log-only, no alert ---
    {{0x24,0x0A,0xC4}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0x30,0xAE,0xA4}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0x24,0x6F,0x28}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0xCC,0x50,0xE3}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0xDC,0x54,0x75}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0xE8,0x9F,0x6D}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0x8C,0xAA,0xB5}, DetectorId::Flock, Confidence::Low, "Flock? (ESP-S3)"},
    {{0x34,0x85,0x18}, DetectorId::Flock, Confidence::Low, "Flock? (ESP-S3)"},
    {{0xD4,0xAD,0xFC}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0xAC,0x67,0xB2}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0x84,0xF3,0xEB}, DetectorId::Flock, Confidence::Low, "Flock? (ESP-S3)"},
    {{0xB4,0xE6,0x2D}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0xCC,0xDB,0xA7}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0x94,0xB9,0x7E}, DetectorId::Flock, Confidence::Low, "Flock? (ESP32)"},
    {{0xA4,0xCF,0x12}, DetectorId::Flock, Confidence::Low, "Flock? (ESP-S2)"},
    {{0xC0,0x49,0xEF}, DetectorId::Flock, Confidence::Low, "Flock? (ESP-C6)"},
};

const OuiSignature *lookupOui(const uint8_t *mac);

// Axon body cameras advertise these SSID prefixes while in pairing mode.
// Source: Axon's own public device-management documentation.
struct SsidPrefixSignature {
    const char *prefix;
    DetectorId detector;
    Confidence confidence;
};

inline constexpr SsidPrefixSignature kSsidPrefixes[] = {
    {"AB2-", DetectorId::Axon, Confidence::High},
    {"AB3-", DetectorId::Axon, Confidence::High},
    {"AB4-", DetectorId::Axon, Confidence::High},
    {"AXON-", DetectorId::Axon, Confidence::High},
};

// The SSID in a beacon is not null-terminated, so this compares against the
// raw bytes and length rather than reaching for strncasecmp.
bool ssidHasPrefix(const uint8_t *ssid, uint8_t ssidLength, const char *prefix);

} // namespace lts
