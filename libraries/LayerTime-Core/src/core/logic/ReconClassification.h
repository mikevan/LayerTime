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

// Core's classification entry point: devices hand captured advertisements
// and frames to core, and core runs the sensor library (LayerTime-Sensors,
// checked out at sensors/) on them. Devices never include the library
// directly; test_boundary enforces that.
//
// Core owns two translations here, and nothing else:
//   * Which detectors are enabled. A device states its Recon selection (a
//     ReconTarget menu value) or its wants() rule, and core turns that into
//     the explicit lts::DetectorSet the classifiers take.
//   * Detector identity. lts::DetectorId and ReconTarget are converted by an
//     explicit mapping in both directions, never by a cast, so neither side
//     can renumber the other.
// Everything else - radio acquisition, scheduling, logging - stays where it
// was.

#if !__has_include(<lts/BleAdvertClassifier.h>)
#error "LayerTime-Sensors is missing: sensors/src/lts was not found. Fetch it with: git submodule update --init sensors"
#endif
#include <lts/BleAdvertClassifier.h>
#include <lts/Signatures.h>
#include <lts/WifiFrameClassifier.h>

#include "ReconCandidate.h"

namespace layertime {
namespace recon {

// What a device's BLE adapter fills in for one advertisement, and the UUID
// table it uses to decide which 16-bit UUIDs are worth reporting.
using lts::BleAdvertSource;
using lts::BleUuidSignature;
using lts::kBleUuidSignatures;

// ReconTarget -> lts::DetectorId. False for None, All, the groups,
// EarlyWarning, and any number that is not a ReconTarget.
bool toSensorDetector(ReconTarget target, lts::DetectorId &out);

// lts::DetectorId -> ReconTarget. ReconTarget::None for a number this core
// does not know (a newer library's detector), so it can never be logged
// under the wrong name.
ReconTarget fromSensorDetector(lts::DetectorId detector);

// The detectors a BLE scan started for `scanSelection` reports: exactly the
// single detectors for which bleScanWants(scanSelection, detector) is true.
lts::DetectorSet bleScanDetectors(ReconTarget scanSelection);

// The single detectors for which `wants` returns true.
lts::DetectorSet enabledDetectors(WantsFn wants, const void *wantsContext);

// Classifies one advertisement for a BLE scan started for `scanSelection`
// and reports each match to `sink` as a core Candidate.
void classifyBleAdvert(const BleAdvertSource &advert, ReconTarget scanSelection,
                       CandidateSink sink, void *sinkContext);

// The Wi-Fi classifier as a device owns it: one per radio, holding the
// library classifier's burst and multi-SSID state.
class WifiFrameClassifier {
public:
    // Forgets every tracked transmitter and BSSID.
    void reset();

    // `wants` is asked, for every detector, which ones this frame is
    // evaluated for; only those run and update their state.
    void classify(const uint8_t *frame, uint16_t length, int8_t rssi, uint8_t channel,
                  uint32_t nowMs, WantsFn wants, const void *wantsContext,
                  CandidateSink sink, void *sinkContext);

private:
    lts::WifiFrameClassifier _sensor;
};

} // namespace recon
} // namespace layertime
