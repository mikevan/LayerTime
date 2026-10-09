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

// Classifies raw 802.11 frames for the Wi-Fi detectors: deauth and disassoc
// bursts, Pwnagotchi, vendor OUI on the BSSID, Axon SSID prefixes, several
// SSIDs from one BSSID, and Pineapple OUIs. Capturing the frames
// (promiscuous mode, channel hopping) is the caller's job; this only reads
// bytes.

#include <stddef.h>
#include <stdint.h>

#include "Candidate.h"

namespace lts {

class WifiFrameClassifier {
public:
    // Deauth burst thresholds. Values follow SquachWatch-CYD's tuning (6
    // frames / 3 s / 15 s cooldown), which is applied globally there; here
    // the same numbers are applied per transmitter, so crossing the
    // threshold is a stronger signal than it is in the original.
    static constexpr uint32_t kDeauthWindowMs = 3000;
    static constexpr uint16_t kDeauthBurstFrames = 6;
    static constexpr uint32_t kDeauthCooldownMs = 15000;

    // Several SSIDs from one BSSID (the evil-portal, Karma, and beacon-spam
    // pattern). A name counts only once it has been heard in
    // kMultiSsidMinBeacons beacons, so one garbled or odd frame can never
    // flag an access point. A name not heard for kMultiSsidStaleMs is
    // dropped, so a router renamed mid-scan stops matching. Hidden (empty or
    // all-zero) names are not names. A detection is reported when a second
    // confirmed name appears and again for each further one, never per
    // beacon, so an event's count is the confirmed names seen.
    static constexpr size_t kMultiSsidTrackers = 16;
    static constexpr uint8_t kMultiSsidNames = 4;
    static constexpr uint8_t kMultiSsidMinBeacons = 3;
    static constexpr uint32_t kMultiSsidStaleMs = 60000;

    // Forgets every tracked transmitter and BSSID.
    void reset();

    // `frame` is the 802.11 frame as captured, `length` its captured length.
    // Only detectors in `enabled` are evaluated, and only they update their
    // burst and multi-SSID tracking.
    void classify(const uint8_t *frame, uint16_t length, int8_t rssi, uint8_t channel,
                  uint32_t nowMs, const DetectorSet &enabled, CandidateSink sink,
                  void *sinkContext);

private:
    // One network name heard from a tracked BSSID.
    struct SsidName {
        bool used = false;
        uint8_t length = 0;
        char name[33] = {0};
        uint8_t beacons = 0;     // capped at kMultiSsidMinBeacons
        bool confirmed = false;  // heard in kMultiSsidMinBeacons beacons
        uint32_t lastSeenMs = 0;
    };

    struct MultiSsidTracker {
        bool used = false;
        uint8_t bssid[6] = {0};
        uint32_t lastSeenMs = 0;
        SsidName names[kMultiSsidNames];
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
                       uint32_t nowMs, const DetectorSet &enabled, CandidateSink sink,
                       void *sinkContext);
    // Records one named beacon from `bssid`. Returns true when that beacon
    // confirmed a name and the BSSID now has two or more confirmed names;
    // `detail` then holds "SSIDs: A | B".
    bool noteMultiSsid(const uint8_t *bssid, const uint8_t *ssid, uint8_t length,
                       uint32_t nowMs, char *detail, size_t detailSize);
    MultiSsidTracker *multiSsidTrackerFor(const uint8_t *bssid, uint32_t nowMs);

    MultiSsidTracker _multiSsid[kMultiSsidTrackers];
    DeauthTracker _deauth[6];
};

} // namespace lts
