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

#include "WifiFrameClassifier.h"

#include <stdio.h>
#include <string.h>

#include "Signatures.h"

namespace lts {

namespace {
void emit(CandidateSink sink, void *context, DetectorId detector, const char *detail,
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

// Empty, or every byte zero: how access points hide their network name.
bool ssidIsHidden(const uint8_t *ssid, uint8_t length)
{
    for (uint8_t i = 0; i < length; ++i)
        if (ssid[i] != 0) return false;
    return true;
}

// Network names are untrusted over-the-air bytes. Shown and written to the
// detection CSV (which does not quote its fields), so anything but printable
// ASCII, and the comma and double quote, become '?'.
void appendSafeName(char *out, size_t outSize, size_t &used, const char *name, uint8_t length)
{
    for (uint8_t i = 0; i < length && used + 1 < outSize; ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        out[used++] = (c < 0x20 || c > 0x7E || c == ',' || c == '"') ? '?' : static_cast<char>(c);
    }
    out[used] = '\0';
}

void appendText(char *out, size_t outSize, size_t &used, const char *text)
{
    for (; *text != '\0' && used + 1 < outSize; ++text) out[used++] = *text;
    out[used] = '\0';
}
}

void WifiFrameClassifier::reset()
{
    for (MultiSsidTracker &tracker : _multiSsid) tracker = MultiSsidTracker{};
    for (DeauthTracker &tracker : _deauth) tracker = DeauthTracker{};
}

void WifiFrameClassifier::classify(const uint8_t *frame, uint16_t length, int8_t rssi,
                                   uint8_t channel, uint32_t nowMs,
                                   const DetectorSet &enabled, CandidateSink sink,
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
    if (enabled.contains(DetectorId::Deauth) && frameType == 0 &&
        (subtype == 0x0C || subtype == 0x0A) && noteDeauthFrame(frame + 10, nowMs))
        // Medium: a real burst pattern, but the threshold and window are
        // judgement calls rather than a signature match.
        emit(sink, sinkContext, DetectorId::Deauth,
             subtype == 0x0C ? "Deauth flood" : "Disassoc flood", mac, rssi,
             Confidence::Medium, channel);
    if (frameType == 0 && subtype == 0x08)
        inspectBeacon(frame, length, rssi, channel, nowMs, enabled, sink, sinkContext);
}

void WifiFrameClassifier::inspectBeacon(const uint8_t *payload, uint16_t length, int8_t rssi,
                                        uint8_t channel, uint32_t nowMs,
                                        const DetectorSet &enabled, CandidateSink sink,
                                        void *sinkContext)
{
    if (length < 38) return;
    const uint8_t *bssid = payload + 10;
    char mac[18];
    formatMac(mac, sizeof(mac), bssid);
    static const uint8_t pwnMac[6] = {0xDE,0xAD,0xBE,0xEF,0xDE,0xAD};
    if (enabled.contains(DetectorId::Pwnagotchi) && memcmp(bssid, pwnMac, 6) == 0)
        // High: an exact match on a fixed, published BSSID.
        emit(sink, sinkContext, DetectorId::Pwnagotchi, "Pwnagotchi beacon", mac, rssi,
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
    if (ouiMatch != nullptr && enabled.contains(ouiMatch->detector))
        emit(sink, sinkContext, ouiMatch->detector, ouiMatch->label, mac, rssi,
             ouiMatch->confidence, channel);

    if (haveSsid) {
        for (const SsidPrefixSignature &sig : kSsidPrefixes) {
            if (!ssidHasPrefix(payload + 38, ssidLength, sig.prefix)) continue;
            if (!enabled.contains(sig.detector)) break;
            char detail[40];
            const int copied = ssidLength < 31 ? ssidLength : 31;
            snprintf(detail, sizeof(detail), "SSID %.*s", copied,
                     reinterpret_cast<const char *>(payload + 38));
            emit(sink, sinkContext, sig.detector, detail, mac, rssi, sig.confidence, channel);
            break;
        }
    }
    if (enabled.contains(DetectorId::MultiSSID) && haveSsid &&
        !ssidIsHidden(payload + 38, ssidLength)) {
        char detail[40];
        if (noteMultiSsid(bssid, payload + 38, ssidLength, nowMs, detail, sizeof(detail)))
            // Medium: some legitimate APs also serve several SSIDs from
            // one BSSID, so this is a pattern rather than proof.
            emit(sink, sinkContext, DetectorId::MultiSSID, detail, mac, rssi,
                 Confidence::Medium, channel);
    }
    if (enabled.contains(DetectorId::Pineapple)) {
        const uint16_t capabilities = static_cast<uint16_t>(payload[34] | (payload[35] << 8));
        if (isPineappleOui(bssid, (capabilities & 0x10) == 0))
            // Medium: an OUI list built from older Hak5 hardware, and some
            // of those prefixes are shared with legitimate vendors.
            emit(sink, sinkContext, DetectorId::Pineapple, "Suspicious Pineapple OUI", mac, rssi,
                 Confidence::Medium, channel);
    }
}

WifiFrameClassifier::MultiSsidTracker *WifiFrameClassifier::multiSsidTrackerFor(
    const uint8_t *bssid, uint32_t nowMs)
{
    MultiSsidTracker *free = nullptr;
    MultiSsidTracker *quietest = nullptr;
    for (MultiSsidTracker &t : _multiSsid) {
        if (!t.used) {
            if (free == nullptr) free = &t;
            continue;
        }
        if (memcmp(t.bssid, bssid, 6) == 0) return &t;
        if (quietest == nullptr ||
            nowMs - t.lastSeenMs > nowMs - quietest->lastSeenMs)
            quietest = &t;
    }
    // A full table gives up the BSSID heard least recently. A rogue access
    // point beacons constantly, so it is never the one given up.
    MultiSsidTracker *slot = free != nullptr ? free : quietest;
    *slot = MultiSsidTracker{};
    slot->used = true;
    memcpy(slot->bssid, bssid, 6);
    return slot;
}

bool WifiFrameClassifier::noteMultiSsid(const uint8_t *bssid, const uint8_t *ssid,
                                        uint8_t length, uint32_t nowMs, char *detail,
                                        size_t detailSize)
{
    MultiSsidTracker *t = multiSsidTrackerFor(bssid, nowMs);
    t->lastSeenMs = nowMs;

    // Names not heard for kMultiSsidStaleMs are forgotten.
    for (SsidName &n : t->names)
        if (n.used && nowMs - n.lastSeenMs > kMultiSsidStaleMs) n = SsidName{};

    SsidName *name = nullptr;
    for (SsidName &n : t->names)
        if (n.used && n.length == length && memcmp(n.name, ssid, length) == 0) {
            name = &n;
            break;
        }
    if (name == nullptr) {
        // A free slot, or else the quietest name not yet confirmed. Confirmed
        // names are kept until they go stale; a fifth name with every slot
        // confirmed is not tracked.
        for (SsidName &n : t->names)
            if (!n.used) {
                name = &n;
                break;
            }
        if (name == nullptr)
            for (SsidName &n : t->names)
                if (!n.confirmed &&
                    (name == nullptr || nowMs - n.lastSeenMs > nowMs - name->lastSeenMs))
                    name = &n;
        if (name == nullptr) return false;
        *name = SsidName{};
        name->used = true;
        name->length = length;
        memcpy(name->name, ssid, length);
    }
    name->lastSeenMs = nowMs;
    if (name->confirmed) return false;
    if (++name->beacons < kMultiSsidMinBeacons) return false;
    name->confirmed = true;

    uint8_t confirmed = 0;
    for (const SsidName &n : t->names)
        if (n.used && n.confirmed) ++confirmed;
    if (confirmed < 2) return false;

    size_t used = 0;
    detail[0] = '\0';
    appendText(detail, detailSize, used, "SSIDs: ");
    bool first = true;
    for (const SsidName &n : t->names) {
        if (!n.used || !n.confirmed) continue;
        if (!first) appendText(detail, detailSize, used, " | ");
        appendSafeName(detail, detailSize, used, n.name, n.length);
        first = false;
    }
    return true;
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
    // Cooldown throttles how often a sustained flood is reported, and so how
    // fast a consumer's repeat count for it climbs. Whether to alert on a
    // report is the consumer's decision, not this classifier's.
    if (tracker->lastFiredMs != 0 && now - tracker->lastFiredMs < kDeauthCooldownMs) return false;
    tracker->lastFiredMs = now;
    return true;
}

} // namespace lts
