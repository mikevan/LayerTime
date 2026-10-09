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

// Confidence, SourceKind, and Band belong to the sensor library
// (LayerTime-Sensors, checked out at sensors/). Core uses them as they are.
#if !__has_include(<lts/Detectors.h>)
#error "LayerTime-Sensors is missing: sensors/src/lts/Detectors.h was not found. Fetch it with: git submodule update --init sensors"
#endif
#include <lts/Detectors.h>

#include "Time.h"

namespace layertime {

// What Recon is pointed at: nothing, everything, a group, one detector, or
// the background early-warning sweep. The values and their order are taken
// unchanged from the T-Ultra's ReconDetector so the adapter is a cast.
// Numeric values are part of the contract. Do not renumber; append only.
//
// A MonitorEvent always carries a single-detector value (Deauth through
// GoogleTag). None, All, the three groups, and EarlyWarning are selections,
// never event sources. The single detectors correspond one to one to the
// sensor library's lts::DetectorId; the conversion is an explicit mapping in
// core/logic/ReconClassification, never a cast.
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

// Confidence, SourceKind, and Band are the sensor library's types, used
// unchanged so a candidate needs no conversion. Their numbers are part of
// LayerTime's contracts (the link records, the stage log), so they are
// pinned here: a library release that renumbered one would stop the build.
// Confidence is ordered: a larger value is a stronger match, and the event
// log relies on that when it keeps the strongest grade an emitter has ever
// matched at.
using Confidence = lts::Confidence;
using SourceKind = lts::SourceKind;
using Band = lts::Band;

static_assert(static_cast<uint8_t>(Confidence::Low) == 0, "Confidence::Low is 0 on the wire");
static_assert(static_cast<uint8_t>(Confidence::Medium) == 1, "Confidence::Medium is 1 on the wire");
static_assert(static_cast<uint8_t>(Confidence::High) == 2, "Confidence::High is 2 on the wire");
static_assert(static_cast<uint8_t>(SourceKind::Unknown) == 0, "SourceKind::Unknown is 0 on the wire");
static_assert(static_cast<uint8_t>(SourceKind::Wifi) == 1, "SourceKind::Wifi is 1 on the wire");
static_assert(static_cast<uint8_t>(SourceKind::Ble) == 2, "SourceKind::Ble is 2 on the wire");
static_assert(static_cast<uint8_t>(SourceKind::Ieee802154) == 3, "SourceKind::Ieee802154 is 3 on the wire");
static_assert(static_cast<uint8_t>(Band::Unknown) == 0, "Band::Unknown is 0 on the wire");
static_assert(static_cast<uint8_t>(Band::Band2_4GHz) == 1, "Band::Band2_4GHz is 1 on the wire");
static_assert(static_cast<uint8_t>(Band::Band5GHz) == 2, "Band::Band5GHz is 2 on the wire");

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
