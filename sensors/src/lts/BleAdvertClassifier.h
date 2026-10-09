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

// Classifies one BLE advertisement for the BLE detectors: Find My trackers,
// Flock's radio module, vendor OUI on the address, and known 16-bit service
// UUIDs. Scanning, and pulling fields out of the BLE stack's advertisement
// object, are the caller's job; this only reads the fields handed to it.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "Candidate.h"

namespace lts {

// One advertisement, as fields. Records and UUIDs are fetched on demand
// through the two callbacks, in index order, so the caller never has
// to copy them all up front.
struct BleAdvertSource {
    // Device name; nameLength 0 when the advertisement carries none.
    const char *name = "";
    size_t nameLength = 0;
    // Address as text, as it appears in the log.
    const char *printedAddress = "";
    // Address bytes exactly as the BLE stack hands them over. The OUI check
    // reads bytes 0 to 2 of this array as the vendor prefix.
    const uint8_t *addressBytes = nullptr;
    int8_t rssi = 0;

    uint8_t manufacturerCount = 0;
    // Copies manufacturer record `index` (company ID first) into `out`.
    void (*manufacturer)(uint8_t index, std::string &out, const void *context) = nullptr;

    uint8_t uuidCount = 0;
    // Service UUID `index` as a 16-bit value. False when it is not a 16-bit
    // UUID, or matches none of kBleUuidSignatures (Signatures.h).
    bool (*uuid16)(uint8_t index, uint16_t &out, const void *context) = nullptr;

    const void *context = nullptr;
};

// Reports each match whose detector is in `enabled`, in this order:
// manufacturer records, then the address OUI, then UUIDs.
void classifyBleAdvert(const BleAdvertSource &advert, const DetectorSet &enabled,
                       CandidateSink sink, void *sinkContext);

} // namespace lts
