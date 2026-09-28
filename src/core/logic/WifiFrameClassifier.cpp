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

#include "WifiFrameClassifier.h"

#include <stdio.h>
#include <string.h>

#include "ReconSignatures.h"

namespace layertime {
namespace recon {

namespace {
void emit(CandidateSink sink, void *context, ReconTarget detector, const char *detail,
          const char *address, int8_t rssi, Confidence confidence, uint8_t channel)
{
    Candidate c;
    c.detector = detector;
    c.detail = detail;
    c.address = address;
    c.rssi = rssi;
    c.confidence = confidence;
    c.channel = channel;
    sink(c, context);
}
}

void WifiFrameClassifier::reset()
{
    _multiSsidCount = 0;
    for (MultiSsidTracker &tracker : _multiSsid) tracker = MultiSsidTracker{};
    for (DeauthTracker &tracker : _deauth) tracker = DeauthTracker{};
}

void WifiFrameClassifier::classify(const uint8_t *frame, uint16_t length, int8_t rssi,
                                   uint8_t channel, uint32_t nowMs, WantsFn wants,
                                   const void *wantsContext, CandidateSink sink,
                                   void *sinkContext)
{
    if (length < 24) return;
    const uint8_t frameType = (frame[0] >> 2) & 0x03;
    const uint8_t subtype = (frame[0] >> 4) & 0x0F;
    char mac[18];
    formatMac(mac, sizeof(mac), frame + 10);

    // noteDeauthFrame() is last in the chain on purpose - it only runs for
    // frames that are actually deauth/disassoc while the detector is live,
    // and it returns true only on a genuine burst.
    if (wants(ReconTarget::Deauth, wantsContext) && frameType == 0 &&
        (subtype == 0x0C || subtype == 0x0A) && noteDeauthFrame(frame + 10, nowMs))
        // Medium: a real burst pattern, but the threshold and window are
        // judgement calls rather than a signature match.
        emit(sink, sinkContext, ReconTarget::Deauth,
             subtype == 0x0C ? "Deauth flood" : "Disassoc flood", mac, rssi,
             Confidence::Medium, channel);
    if (frameType == 0 && subtype == 0x08)
        inspectBeacon(frame, length, rssi, channel, wants, wantsContext, sink, sinkContext);
}

void WifiFrameClassifier::inspectBeacon(const uint8_t *payload, uint16_t length, int8_t rssi,
                                        uint8_t channel, WantsFn wants,
                                        const void *wantsContext, CandidateSink sink,
                                        void *sinkContext)
{
    if (length < 38) return;
    const uint8_t *bssid = payload + 10;
    char mac[18];
    formatMac(mac, sizeof(mac), bssid);
    static const uint8_t pwnMac[6] = {0xDE,0xAD,0xBE,0xEF,0xDE,0xAD};
    if (wants(ReconTarget::Pwnagotchi, wantsContext) && memcmp(bssid, pwnMac, 6) == 0)
        // High: an exact match on a fixed, published BSSID.
        emit(sink, sinkContext, ReconTarget::Pwnagotchi, "Pwnagotchi beacon", mac, rssi,
             Confidence::High, channel);

    // The SSID is the first information element after the 24-byte header and
    // the 12 bytes of fixed parameters: tag ID at [36], length at [37], data
    // from [38]. 802.11 puts the SSID first, but verify the tag rather than
    // assume it - otherwise a frame whose first element is something else
    // has an unrelated byte read as an SSID length.
    const bool haveSsid = payload[36] == 0x00 && payload[37] <= 32 &&
                          38U + payload[37] <= length;
    const uint8_t ssidLength = haveSsid ? payload[37] : 0;

    // Vendor OUI on the beacon's BSSID. The Flock and Axon prefixes are
    // Wi-Fi MAC blocks, so this is their natural home - the BLE-address
    // check is the secondary path, not the main one.
    const OuiSignature *ouiMatch = lookupOui(bssid);
    if (ouiMatch != nullptr && wants(ouiMatch->detector, wantsContext))
        emit(sink, sinkContext, ouiMatch->detector, ouiMatch->label, mac, rssi,
             ouiMatch->confidence, channel);

    if (haveSsid) {
        for (const SsidPrefixSignature &sig : kSsidPrefixes) {
            if (!ssidHasPrefix(payload + 38, ssidLength, sig.prefix)) continue;
            if (!wants(sig.detector, wantsContext)) break;
            char detail[40];
            const int copied = ssidLength < 31 ? ssidLength : 31;
            snprintf(detail, sizeof(detail), "SSID %.*s", copied,
                     reinterpret_cast<const char *>(payload + 38));
            emit(sink, sinkContext, sig.detector, detail, mac, rssi, sig.confidence, channel);
            break;
        }
    }
    if (wants(ReconTarget::MultiSSID, wantsContext) && haveSsid) {
        MultiSsidTracker *tracker = nullptr;
        for (size_t i = 0; i < _multiSsidCount; ++i)
            if (memcmp(_multiSsid[i].bssid, bssid, 6) == 0) { tracker = &_multiSsid[i]; break; }
        if (!tracker && _multiSsidCount < 8) {
            tracker = &_multiSsid[_multiSsidCount++];
            memcpy(tracker->bssid, bssid, 6);
        }
        if (tracker) {
            const uint16_t hash = ssidLength ? hashSsid(payload + 38, ssidLength) : 0xFFFF;
            bool known = false;
            for (uint8_t i = 0; i < tracker->count; ++i) if (tracker->hashes[i] == hash) known = true;
            if (!known && tracker->count < 4) tracker->hashes[tracker->count++] = hash;
            if (tracker->count >= 2)
                // Medium: some legitimate APs also serve several SSIDs from
                // one BSSID, so this is a pattern rather than proof.
                emit(sink, sinkContext, ReconTarget::MultiSSID, "Multiple SSIDs from BSSID", mac,
                     rssi, Confidence::Medium, channel);
        }
    }
    if (wants(ReconTarget::Pineapple, wantsContext)) {
        const uint16_t capabilities = static_cast<uint16_t>(payload[34] | (payload[35] << 8));
        if (isPineappleOui(bssid, (capabilities & 0x10) == 0))
            // Medium: an OUI list built from older Hak5 hardware, and some
            // of those prefixes are shared with legitimate vendors.
            emit(sink, sinkContext, ReconTarget::Pineapple, "Suspicious Pineapple OUI", mac, rssi,
                 Confidence::Medium, channel);
    }
}

bool WifiFrameClassifier::noteDeauthFrame(const uint8_t *mac, uint32_t now)
{
    DeauthTracker *tracker = nullptr;
    for (DeauthTracker &candidate : _deauth) {
        if (candidate.used && memcmp(candidate.mac, mac, 6) == 0) {
            tracker = &candidate;
            break;
        }
    }

    if (tracker == nullptr) {
        // Unseen transmitter: take a free slot, or evict whichever tracked
        // transmitter has been quiet longest. The table is deliberately small
        // - a real flood comes from one or two sources, and anything larger
        // would just be remembering background noise.
        tracker = &_deauth[0];
        for (DeauthTracker &candidate : _deauth) {
            if (!candidate.used) {
                tracker = &candidate;
                break;
            }
            if (now - candidate.windowStartMs > now - tracker->windowStartMs) tracker = &candidate;
        }
        *tracker = DeauthTracker{};
        memcpy(tracker->mac, mac, 6);
        tracker->used = true;
        tracker->windowStartMs = now;
    }

    // Rolling window that restarts after a gap longer than itself, rather
    // than a fixed repeating interval - so isolated frames minutes apart
    // never accumulate into a burst, and a flood straddling a boundary is
    // not split into two halves that each miss the threshold.
    if (now - tracker->windowStartMs > kDeauthWindowMs) {
        tracker->windowStartMs = now;
        tracker->count = 0;
    }
    if (tracker->count < 0xFFFF) ++tracker->count;

    if (tracker->count < kDeauthBurstFrames) return false;
    // Cooldown throttles how fast a sustained flood drives the detection's
    // encounterCount up. It does not gate the alert itself - addDetection()
    // only raises alertPending for a genuinely new (category, address).
    if (tracker->lastFiredMs != 0 && now - tracker->lastFiredMs < kDeauthCooldownMs) return false;
    tracker->lastFiredMs = now;
    return true;
}

} // namespace recon
} // namespace layertime
