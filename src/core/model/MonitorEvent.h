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

// Canonical definition: contracts/models.md, "MonitorEvent".

#include <stdint.h>

#include "Time.h"

namespace layertime {

// What Recon is pointed at: nothing, everything, a group, one detector, or
// the background early-warning sweep. The values and their order are taken
// unchanged from the T-Ultra's ReconDetector so the adapter is a cast.
// Numeric values are part of the contract. Do not renumber; append only.
//
// A MonitorEvent always carries a single-detector value (Deauth through
// GoogleTag). None, All, the three groups, and EarlyWarning are selections,
// never event sources.
enum class ReconTarget : uint8_t {
    None = 0,
    All = 1,
    Trackers = 2,
    CounterSurveil = 3,
    CounterIntrusion = 4,
    Deauth = 5,
    Pwnagotchi = 6,
    MultiSSID = 7,
    Flock = 8,
    Pineapple = 9,
    AirTag = 10,
    Flipper = 11,
    Meta = 12,
    Axon = 13,
    Tile = 14,
    SamsungTag = 15,
    GoogleTag = 16,
    EarlyWarning = 17,
};

// Values match the T-Ultra's SignalConfidence. Ordered: a larger value is a
// stronger match, and the T-Ultra relies on that ordering when it keeps the
// strongest grade an emitter has ever matched at.
enum class Confidence : uint8_t {
    Low = 0,
    Medium = 1,
    High = 2,
};

enum class SourceKind : uint8_t {
    Unknown = 0,
    Wifi = 1,
    Ble = 2,
    Ieee802154 = 3,
};

enum class Band : uint8_t {
    Unknown = 0,
    Band2_4GHz = 1,
    Band5GHz = 2,
};

// One emitter matched by one detector. Repeat sightings of the same emitter
// by the same detector update this record (count, rssi, lastSeen) rather
// than creating a new one.
//
// The display name of a detector is not stored here. It comes from the
// canonical name table in contracts/vectors/enums.json, so a client can show
// an event without knowing anything about the detector that produced it.
struct MonitorEvent {
    static constexpr uint8_t kDetailSize = 40;
    static constexpr uint8_t kSourceIdSize = 19;

    // Assigned by the producer when the record is created. Increases by one
    // per new record. Never reused within one boot of the producer.
    uint32_t eventId = 0;

    ReconTarget detector = ReconTarget::None;
    Confidence confidence = Confidence::High;

    SourceKind sourceKind = SourceKind::Unknown;
    // Observed identifier, usually a MAC as "AA:BB:CC:DD:EE:FF". Observed
    // over the air, so it is untrusted text.
    char sourceId[kSourceIdSize] = {0};
    // Detector-specific human-readable detail, e.g. an SSID or vendor.
    char detail[kDetailSize] = {0};

    int8_t rssi = 0;
    // 0 when the channel is unknown or not applicable (BLE).
    uint8_t channel = 0;
    Band band = Band::Unknown;

    // Sightings of this emitter by this detector, including the first.
    uint32_t count = 1;

    Timestamp lastSeen;
};

} // namespace layertime
