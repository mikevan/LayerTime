# Wi-Fi frame evidence on Android: finding and proposal

Status: investigation complete, proposal for review. No contract or LayerWand
change is implemented. This gates the Wireshark-grade attack-detail screen.

## Finding: the wand captures raw frames but keeps none, and Link carries none

Inspected `devices/lilygo-layerwand/src/C5ReconRadio.cpp`, the LayerTime Link
contract, and the Android controller.

- The wand runs promiscuous Wi-Fi capture (`esp_wifi_set_promiscuous(true)`,
  `C5ReconRadio.cpp:150-161`). In `promiscuousThunk` (`:231-253`) it receives a
  `wifi_promiscuous_pkt_t` with `packet->payload` (the raw 802.11 frame) and
  `packet->rx_ctrl` (`channel`, `rssi`, `sig_len`). It passes those straight to
  `_wifiClassifier.classify(payload, sig_len, rssi, channel, millis(), ...)` and
  returns. The raw bytes are never stored. There is no frame ring buffer.
- The only per-frame receiver metadata the ESP32 provides here is `rx_ctrl`:
  channel, RSSI, and `sig_len`. There is no FCS, noise, antenna, or full
  Radiotap available from `esp_wifi` promiscuous `rx_ctrl`.
- LayerTime Link 0.1 transports only the normalized `EVENT_SUMMARY` (detector,
  confidence, sourceKind, band, channel, RSSI, count, ageSeconds, flags) plus
  `sourceId`/`detail` text. Frames are capped at 20 bytes. There is no raw-frame
  transport.

Conclusion: Android receives zero raw frame bytes today. Wireshark has nothing
to decode. The detail screen is honest about this: every frame, address,
Radiotap, and raw-hex field is marked UNAVAILABLE with that reason, and nothing
is fabricated.

## What the Android detail screen shows now

- Real, labeled by provenance: classification and confidence (INFERRED by the
  wand classifier), `sourceId`/`detail` (DECODED over-the-air text, untrusted),
  channel/band/RSSI (receiver METADATA), count/age/observation window
  (CALCULATED), wand identity and connection (METADATA).
- UNAVAILABLE with reason: TA/RA/SA/DA/BSSID, broadcast/multicast, frame
  type/subtype, reason code, sequence/fragment/retry/protected flags,
  length/FCS, information elements, per-frame timestamps, noise/rate/PHY/antenna,
  captured-frame count and rate, truncation and dropped counts, RSSI and
  frame-rate series, multi-wand correlation, the Wireshark tree, raw hex, and
  PCAP export.

## Capture identity: what the evidence key must be (verified)

Before keying evidence on `(sessionId, eventId)`, I verified what those values
mean and how long they live. Sources: `contracts/link.md` session rule 6,
`C5Link.cpp`.

- **`sessionId` is 16-bit and per-power-on.** `C5Link::begin()` sets
  `_identity.sessionId = nonZeroSessionId()` **once** at boot;
  `nonZeroSessionId()` is `esp_random() & 0xFFFF` retried until non-zero
  (`C5Link.cpp:79-84, 102`). It is generated in `begin()`, not in the
  connect/advertise path, so:
  - it **survives a BLE disconnect/reconnect** — the same value is held in RAM
    across the drop, which is exactly what lets the interface re-pull from
    `lastSeenChangeSeq` after a reconnect;
  - it **changes on every wand reboot/power-cycle** — a fresh `esp_random()`.
    Contract rule 6: "A changed `sessionId` means a new Recon session: the
    interface discards its mirror and tells the wearer."
- **`sessionId` is not unique enough to stand alone.** 16 bits over repeated
  reboots will collide (birthday bound: a ~1% chance of a repeat within ~36
  reboots, rising from there). Two different capture sessions can therefore
  carry the same `sessionId`. `(sessionId, eventId)` is unique **only within one
  power-on of one wand** — it is not a cross-reboot or cross-wand key.
- **The wand has a stable hardware identity.** `C5Link.cpp:111`
  `esp_read_mac(mac, ESP_MAC_BT)` reads the burned-in BT MAC; the advertised
  name is `LT-C5-` + the last two bytes (`BringUpLogic.h:105-106`). The ESP32
  uses a public (static) BT address, so the Android central sees a **stable peer
  address** across disconnects and reboots.
- **`wandBtMac` alone does not fix the reboot collision.** The MAC is constant
  for a given wand, so for *that same wand* two different boots that happen to
  draw the same 16-bit `sessionId` still produce the same
  `(wandBtMac, sessionId, eventId)` — the MAC disambiguates *different wands*,
  not *different boots of one wand*. A collision-free key needs a value that
  never repeats across boots of the same device.

**A new boot/capture identifier is required (does not touch the Garmin field).**
- **`bootCount`: a persistent, monotonic per-boot counter.** Store a `uint32`
  in ESP32 **NVS** (the flash key/value store, survives power loss), increment
  it once in `begin()`, and never reset it. It never repeats for a wand across
  4.29 billion boots, so it eliminates the reboot collision that `sessionId`
  cannot. (Optionally pair it with a one-time random `installId` drawn and
  stored in NVS on first boot, to also survive an NVS erase/re-flash; `bootCount`
  alone is sufficient for the reboot-collision problem stated here.)
- **The existing 16-bit `sessionId` field is unchanged.** It stays exactly as
  Garmin reads it today (the "new Recon session → discard mirror" signal, which
  only needs to detect a reboot during one wearing, where a 16-bit value is
  fine). `bootCount` is an **additional** field carried only in the new evidence
  index (`GET_FRAMES`), never in `HELLO_ACK`, so the Garmin handshake and its
  `sessionId` semantics are untouched.

**Proposed evidence key: `(wandBtMac, bootCount, eventId)`**, with `sessionId`
carried alongside for continuity with the Garmin session semantics.
- `wandBtMac` (6 bytes, already known to the central) pins evidence to a
  physical wand and separates wands from each other.
- `bootCount` separates boots of the *same* wand with no collision — this is the
  piece `(wandBtMac, sessionId)` was missing.
- `eventId` identifies the event within that boot.
- `sessionId` is still recorded per event so a session change is visible and so
  the evidence lines up with what Garmin saw, but it is **not** load-bearing for
  uniqueness.
- The controller records, per stored event, the wall-clock time it first saw the
  event plus `(bootCount, sessionId)`, so a reboot mid-investigation is a visible
  boundary rather than silently renumbered or silently merged evidence.

This also answers use-case-1 multi-wand correlation only partially: the key
identifies *which wand* saw an event, but it is **not** a cross-wand emitter
identity. Correlating the same attacker across wands still needs a transmitter
fingerprint (the TA/BSSID from a decoded frame) — see section B and
`DEAUTH_ANALYSIS_GAPS.md`.

## Proposal for review (implement only after you approve)

### A. LayerWand: a bounded raw-frame evidence buffer with burst context
A single triggering frame cannot establish burst behavior, so retention must
keep context, not just the trigger.

- **What to retain per flagged event** (deauth and disassociation first): the
  triggering 802.11 frame bytes (up to `sig_len`) **plus a bounded selection of
  surrounding frames** — a short window before and after the trigger on the same
  channel — so burst rate and duration can be measured rather than asserted.
- **Per-frame record:** channel, RSSI, `sig_len`, captured length (may be < the
  on-air length), a monotonic capture timestamp, a per-frame `truncated` flag, a
  per-frame `fcsPresent` flag, and the `(wandBtMac, bootCount, eventId)` link
  (`sessionId` carried alongside).
- **Retained length and FCS (must be defined, not assumed).** The ESP32
  promiscuous callback hands over `rx_ctrl.sig_len` bytes in `payload`; by the
  documented esp_wifi behavior `sig_len` is the on-air length **including the
  4-byte FCS trailer**. So the wand must record, per frame: `onAirLen` =
  `sig_len`; `capturedLen` = `min(sig_len, slotBytes)` (what is actually stored);
  `truncated` = `capturedLen < onAirLen`; and `fcsPresent` = the last 4 bytes of
  the stored frame are the FCS (true only when the frame was stored whole,
  `capturedLen == onAirLen`). The decoder is handed `fcsPresent` so it treats the
  trailing 4 bytes as FCS and not as frame body (our host spike decoded a
  fixture with **no** FCS and set `wlan.check_fcs:FALSE`; real wand frames carry
  one and must be decoded accordingly). The exact `sig_len`/FCS semantics vary by
  IDF version, so the wand spike **confirms this on-device** (capture a known
  frame, compare `sig_len` to the decoded length) before the lengths are trusted
  — it is a measurement, not an assumption.
- **Structure and overflow:** a fixed-size ring buffer, for example 64 frames ×
  256 bytes per event-evidence slot, with a fixed number of concurrent slots.
  When capture outruns the buffer (a real flood will), the oldest frames in the
  window are overwritten. The wand must then report, per event:
  - `framesSeen` — total frames the capture callback counted in the window;
  - `framesRetained` — how many are actually held;
  - `framesDropped` = `framesSeen − framesRetained` (ring overwrite);
  - `bytesTruncated` — count of frames stored shorter than their on-air length.
  Evidence is explicitly incomplete, never silently so. The detail screen shows
  these counters and labels the window "partial" when anything was dropped or
  truncated.
- Measure RAM/flash budget on the T-Dongle-C5, capture throughput, and real
  dropped-frame counts under a deauth burst **before** promising any retention
  depth. Prefer bounded retention with selective retrieval over continuous raw
  streaming; BLE throughput is far below the capture rate, so the window size is
  bounded by what BLE can later retrieve as much as by RAM.

### B. Link 0.2: backward-compatible evidence transfer
Additive only, so Garmin and the existing ops keep working:
- `GET_FRAMES(eventId)`: a frame **index** that carries the evidence key
  (`wandBtMac` is already known; echo `bootCount`, `sessionId`, `eventId`), the
  counters from section A (`framesSeen`, `framesRetained`, `framesDropped`,
  `bytesTruncated`),
  and per frame: captured length, on-air length, channel, RSSI, capture
  timestamp (monotonic, wand-relative — timestamps still do not cross as
  wall-clock, consistent with rule 5), and per-frame `truncated`/`fcsPresent`
  flags.
- `GET_FRAME_BYTES(eventId, index)`: the raw bytes in fragments ending with END,
  the same pattern as GET_TEXT but binary; a short frame returns fewer bytes and
  the index's `truncated` flag already said so.
- A `localRawCapture` capability bit in HELLO_ACK so interfaces discover
  support. **Verified safe for Garmin:** `LinkClient.mc:644` computes
  `reconAvailable = (capabilities & (CAP_LOCAL_WIFI_MONITOR |
  CAP_LOCAL_BLE_MONITOR)) != 0` — it masks only those two bits, so a new bit at a
  different position is ignored by the existing Garmin client.
- `EVENT_SUMMARY` is unchanged. Add byte-exact vectors to
  `contracts/vectors/link_frames.json`. This is a deliberate contract change,
  reviewed per `contracts/README.md` and LT-ARCH-001 section 16.3.

### C. Android decoding
- libwireshark/epan via the NDK: Wireshark is GPL-2.0-or-later, and the
  LayerTime sources carry GPL-3.0 headers, so the licenses are compatible and
  the combined APK is GPL-3.0 with source offered. Cost is real: libwireshark
  pulls in GLib and a large cross-compile, and the APK grows by tens of MB. See
  the separate feasibility assessment (below) for actual, measured numbers.
- I will not silently substitute one decoder for another. The feasibility
  assessment is input to your decision; the recommendation there is explicit and
  yours to accept or reject.
- Do not fabricate Radiotap fields. Show only `rx_ctrl`-provided metadata.

### D. Validation plan
- **Decoding — desktop fixtures:** deauth, disassociation, a legitimate
  disconnect, retries, malformed or truncated frames, and a protected management
  frame whose body cannot be decoded without keys. Decode with the chosen
  decoder and with TShark, record the Wireshark version, and compare field by
  field.
- **Decoding — on device:** capture real frames on the wand, retrieve them with
  `GET_FRAME_BYTES`, decode on Android, and compare to TShark on the same bytes.
- **Garmin compatibility (two gates, both required):**
  1. *Necessary:* the existing codec conformance vectors in
     `contracts/vectors/link_frames.json` still pass against the Monkey C
     binding after the 0.2 additions. This proves the additive frames did not
     perturb existing frame encodings — but unchanged existing operations do not
     by themselves prove a real wearer-facing connection still works.
  2. *Sufficient:* a **real Garmin tactix ↔ wand connection** against a wand
     running the 0.2 firmware: HELLO/HELLO_ACK handshake completes, the Garmin
     reports `reconAvailable` correctly (it should ignore `localRawCapture` and
     set availability purely from the WIFI/BLE monitor bits), GET_CHANGED /
     GET_TEXT keep working, and recon events still annunciate on the watch.
     Record the watch model and firmware.

## Follow-on requirement (retained, not this milestone): response assessment

Decoding a frame establishes *what was seen*, never *what was done about it*.
A separate, later capability must answer, for a given detection: (1) is there an
applicable defensive response at all; (2) what capabilities would it require
(and does this platform have them — a passive monitor has none that transmit);
and (3) how would its effectiveness be verified by observation, not asserted.
**Hard rule: decoding a frame, or classifying a detection, must never produce a
"countered" / "mitigated" / "blocked" claim.** The detail screen may show that a
response is *possible* or *unavailable*, never that one *happened*, unless
effectiveness was independently observed. This does not gate the decoding
milestone; it is recorded here so the decode work does not drift into implying
countermeasures it never performed. This preserves the passive-only posture
(LT-ARCH-001 §2.6, §14, §16.7).

## Cautions
- Do not derive distance, bearing, location, or verified attacker identity from
  MAC or RSSI alone.
- A reason code states the sender's claimed reason, not proof of an attack.
- A single retained frame is a sighting, not a burst; only the counted window
  with explicit dropped/truncated numbers supports a burst claim.
- Keep the Garmin protocol working, keep Android code isolated, and do not root
  or flash either phone.
