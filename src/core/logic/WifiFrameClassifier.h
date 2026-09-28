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

// Classifies raw 802.11 frames for the Wi-Fi Recon detectors: deauth and
// disassoc bursts, Pwnagotchi, vendor OUI on the BSSID, Axon SSID prefixes,
// several SSIDs from one BSSID, and Pineapple OUIs. Moved unchanged out of
// ReconService in Phase 0 Step 3e. Capturing the frames (promiscuous mode,
// channel hopping) stays on the platform side; this only reads bytes.

#include <stddef.h>
#include <stdint.h>

#include "ReconCandidate.h"

namespace layertime {
namespace recon {

class WifiFrameClassifier {
public:
    // Deauth burst thresholds. Values follow SquachWatch-CYD's tuning (6
    // frames / 3 s / 15 s cooldown), which is applied globally there; here
    // the same numbers are applied per transmitter, so crossing the
    // threshold is a stronger signal than it is in the original.
    static constexpr uint32_t kDeauthWindowMs = 3000;
    static constexpr uint16_t kDeauthBurstFrames = 6;
    static constexpr uint32_t kDeauthCooldownMs = 15000;

    // Forgets every tracked transmitter and BSSID.
    void reset();

    // `frame` is the 802.11 frame as captured, `length` its captured length.
    void classify(const uint8_t *frame, uint16_t length, int8_t rssi, uint8_t channel,
                  uint32_t nowMs, WantsFn wants, const void *wantsContext,
                  CandidateSink sink, void *sinkContext);

private:
    struct MultiSsidTracker {
        uint8_t bssid[6] = {0};
        uint16_t hashes[4] = {0};
        uint8_t count = 0;
    };

    // Per-transmitter deauth/disassoc rate tracking. A single frame is
    // ordinary Wi-Fi traffic - a phone leaving a network, an AP restarting,
    // a roaming handoff - so a detection needs a burst from one source,
    // not one frame from anywhere. Tracked per transmitter rather than
    // globally so unrelated background deauths across several APs can't
    // add up into a phantom flood.
    struct DeauthTracker {
        bool used = false;
        uint8_t mac[6] = {0};
        uint32_t windowStartMs = 0;
        uint32_t lastFiredMs = 0;
        uint16_t count = 0;
    };

    // Records one deauth/disassoc frame from `mac`. Returns true only when
    // that transmitter has crossed the burst threshold and is outside the
    // re-fire cooldown - i.e. when this is worth reporting as a flood.
    bool noteDeauthFrame(const uint8_t *mac, uint32_t now);
    void inspectBeacon(const uint8_t *payload, uint16_t length, int8_t rssi, uint8_t channel,
                       WantsFn wants, const void *wantsContext, CandidateSink sink,
                       void *sinkContext);

    MultiSsidTracker _multiSsid[8];
    size_t _multiSsidCount = 0;
    DeauthTracker _deauth[6];
};

} // namespace recon
} // namespace layertime
