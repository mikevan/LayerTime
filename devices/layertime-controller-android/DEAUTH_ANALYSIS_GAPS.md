# Fields needed for expanded deauth analysis vs. LayerTime Link 0.1

This note identifies the data the owner's expanded deauth use cases need that
**contract 0.1 does not supply**. Nothing here is implemented: adding any of it
is a contract change, reviewed before any binding changes, per
`contracts/README.md` ("Changing the contract") and LT-ARCH-001 section 16.3.
This milestone displays only the fields the contract already carries.

## What contract 0.1 supplies today (per event)

From `EVENT_SUMMARY` + GET_TEXT + Status:

- detector (DEAUTH is `ReconTarget` 5), confidence (LOW / MED / HIGH only),
  sourceKind, band, channel, RSSI (last-seen, signed i8), sighting count
  (saturating u16), age in seconds (saturating u16), `sourceId` text (≤18
  chars), `detail` text (≤39 chars), eventId, sessionId.
- Status: monitoring flags, event count, active target, changeSeq, heartbeat.

That is enough to **detect and annunciate** an incoming deauth condition and to
show its channel, rough signal strength, and how often it is being seen. It is
not enough for the three expanded use cases.

## Use case 1 — attack vector and distance from multiple wands

Missing from the contract:

- **Per-wand position.** `EVENT_SUMMARY` carries no GPS. NAV in the architecture
  is interface-to-Node only; a wand does not report its own location over the
  link. Triangulating a bearing or estimating range needs each wand's position.
- **A stable cross-wand emitter key.** `eventId` is per-session-per-node, so the
  same attacker seen by four wands produces four unrelated eventIds. Correlating
  them needs a stable fingerprint (for Wi-Fi, the transmitter MAC/BSSID) present
  in every wand's event.
- **Comparable, timestamped RSSI.** Only a single last-seen RSSI and an age
  cross the link. A distance/vector estimate wants synchronized RSSI samples (a
  short time series) with a common time base. The contract rule "timestamps
  never cross the link" blocks synchronized timing as written.

## Use case 2 — attacker characterization (ESP-IDF / Scapy / Wireshark)

Missing from the contract:

- **Attacker and target MAC/BSSID** as structured fields. `sourceId` is free
  text, ≤18 chars, and untrusted; it is not a typed address pair.
- **802.11 reason code** for the deauth/disassociation.
- **Frame sequence numbers** and **per-frame timing** to measure burst rate and
  duration (what separates a true flood from incidental frames).
- **Raw frame bytes / a PCAP channel.** Scapy and Wireshark operate on captured
  802.11 frames. The link carries normalized summaries, not raw captures. A new
  bulk/opaque transfer path would be required.

## Use case 3 — rich expert display

Depends on the above. With structured addresses, reason codes, burst
timing/rate, an RSSI series, and wand positions, the phone can render a far
richer view. None of those inputs exist in contract 0.1.

## Recommendation

Keep this milestone on contract 0.1 (detect, observe, log, export). Propose the
additions above as a scoped Link 0.2 for review: a structured Wi-Fi management
sub-record on `EVENT_SUMMARY` (transmitter, receiver, reason, seq, burst
window), a wand-position/time element, and an optional raw-frame transfer op.
Each is a deliberate contract change with its own test vectors, not an
implementation detail to add unilaterally.
