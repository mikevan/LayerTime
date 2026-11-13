# LayerTime Link 0.2 design: frame evidence and HUNT

Status: design approved by Michael on 2026-10-30. Not implemented. This is the design, not yet the wire contract: Phase 2 turns Part 3 into the Link 0.2 specification with byte-exact vectors, alongside `link.md`, and each phase is reviewed before the next starts.

This replaces the plan part of `devices/layertime-controller-android/FRAME_EVIDENCE_PROPOSAL.md`. That proposal's findings, its evidence key, and its cautions still stand and are summarized here. What's new:
- HUNT mode, the fox-hunt features built on it, and the watch's real sensors;
- Bluetooth evidence alongside Wi-Fi;
- LayerTime-Sensors handing over the frame behind each match;
- a larger-frame option for the Android watch;
- a measurement phase before anything is promised.

## Goals

1. **Deeper Wireshark analysis on the watch.** Bring the raw frames behind a detection from the Hunter to the watch, so Wireshark decodes the real evidence instead of an imported file.
2. **A true fox hunt.**
   - Lock onto one emitter and get fast signal strength.
   - Turn that into a direction and a rough location with the hardware the watch actually has.
   - Support both outcomes equally: close on it, or know where not to walk.

## What the hardware gives us (measured)

| Fact | Evidence |
|---|---|
| The Hunter sees every raw Wi-Fi frame and keeps none | `C5ReconRadio.cpp` passes `packet->payload` to the classifier and returns (FRAME_EVIDENCE_PROPOSAL.md) |
| Link 0.1 frames are 20 bytes or less | `contracts/link.md` rule 1. The Garmin's Connect IQ BLE API writes at most 20 bytes. |
| The Android watch does not request a larger BLE MTU today | No `requestMtu` anywhere in the controller |
| Garmin ignores unknown capability bits | `LinkClient.mc:644` masks only `CAP_LOCAL_WIFI_MONITOR \| CAP_LOCAL_BLE_MONITOR` |
| The watch is running Wireshark 4.4.19 | `wireshark-prebuilt/include/config.h` |
| The watch's sensors are an accelerometer, a light sensor, a barometer, a temperature sensor (which never reports), a step detector and counter, heart rate, and a tilt detector | `dumpsys sensorservice` on the LOKMAT watch, 2026-10-30 |
| No magnetometer and no gyroscope, so no compass and no heading | Same; every Android fusion mode shows "disabled (0 clients)" |
| The Hunter hands NimBLE's address over backwards, so the library's Bluetooth prefix checks never match | `C5ReconRadio.cpp:283` KNOWN_DEFECT; LayerTime-Sensors `test_api_examples` proves the rule |

## Rules that do not change

- **Passive only.** Nothing in this design transmits toward a target.
- **Garmin keeps working.** Every Link addition is a new op, a new frame type, or a new capability bit. The Garmin sends nothing new and ignores what it doesn't know.
- **Evidence is never silently incomplete.** Every window reports how many frames were seen, kept, dropped, and cut short.
- **No distance, bearing, or location from signal strength without its uncertainty.** Every arrow and map area shows its error range, labeled as an estimate.
- **Decoding never claims a response.** No "countered", "mitigated", or "blocked" (FRAME_EVIDENCE_PROPOSAL.md, response assessment).

## Part 1: LayerTime-Sensors 1.2.0, the frame behind each match

Each `Candidate` gains two fields: `frame` (const uint8_t pointer) and `frameLength`. They point at the exact bytes that matched, and are valid only during the sink call, like `detail`.

- **Wi-Fi:** the 802.11 frame passed to `classify()`.
- **Bluetooth:** the raw advertisement. `BleAdvertSource` gains optional `payload` and `payloadLength` fields; NimBLE provides them with `getPayload()`. Left empty, `frame` is null, so existing callers keep compiling.
- **API and tests:** `API.md` documents both. A test proves the pointer is the right bytes and is null when no payload is given.
- **Why the library:** the Hunter needs this to know which bytes to keep, and every other user of the library gets a way to save evidence.

## Part 2: Hunter evidence buffer

As in FRAME_EVIDENCE_PROPOSAL.md section A, with Bluetooth added.

**Evidence key:** `(wandBtMac, bootCount, eventId)`. `bootCount` is a 32-bit counter kept in NVS and incremented once per boot, because the 16-bit `sessionId` repeats across reboots. `sessionId` travels alongside but isn't relied on for uniqueness.

**Per event, a bounded window:** the triggering frame plus a few frames before and after, from the same transmitter on the same channel.
- **Per frame:** channel, signal, on-air length, captured length, the truncated and FCS-present flags, and a monotonic capture time in milliseconds relative to the wand's clock. Timestamps still never cross as wall-clock time (rule 5).
- **Per window:** `framesSeen`, `framesRetained`, `framesDropped`, and `bytesTruncated`.
- **Bluetooth:** advertisements are at most 31 bytes, or more for extended advertising. Keep the last N advertisements from the same address, with signal and time.

Sizes are set from Phase 0 measurements, not guessed.

## Part 3: Link 0.2 additions

All new ops reply on Data. Every frame stays 20 bytes or less unless the interface asks for more (see Larger frames below). Every request still gets exactly one reply, or a run that ends with END.

### Capabilities (HELLO_ACK)

| Bit | Name | Meaning |
|---|---|---|
| 5 | rawCapture | The Node keeps evidence windows and answers GET_FRAMES and GET_FRAME_BYTES |
| 6 | hunt | The Node answers HUNT_START, HUNT_STOP, and GET_HUNT |
| 9 | cardRead | The Node answers CARD_LIST and CARD_READ (2026-11-04) |
| 10 | cardClear | The Node answers CARD_CLEAR (2026-11-06) |

### Status

- Flags bit 6, **hunting**: a hunt is active.
- `active` keeps its meaning. HUNT pauses early warning, the way manual detector mode already does.

### Evidence ops

| Op | Value | Length | Arguments | Reply |
|---|---|---|---|---|
| GET_FRAMES | `0x06` | 6 | eventId u32 | FRAME_WINDOW, then FRAME_INDEX..., END |
| GET_FRAME_BYTES | `0x07` | 9 | eventId u32, index u8, offset u16 | FRAME_BYTES..., END |

- **FRAME_WINDOW** (type `0x87`): `bootCount u32`, `sessionId u16`, `framesSeen u16`, `framesRetained u8`, `framesDropped u16`, `bytesTruncated u8`, and source kind.
- **FRAME_INDEX** (type `0x88`): one per kept frame. Index, channel, signal, on-air length, captured length, flags (truncated, FCS present, triggering frame), and the capture-time offset in milliseconds from the triggering frame.
- **FRAME_BYTES** (type `0x89`): raw bytes from `offset`. `offset` lets a reconnecting interface resume a long frame where it stopped.
- **Behavior:** an event with no evidence window returns RESULT NotReady. An event that has expired from the 40-event log returns RESULT InvalidArgument.

### HUNT ops

| Op | Value | Length | Arguments | Reply |
|---|---|---|---|---|
| HUNT_START | `0x08` | 10 | sourceKind u8, address 6 bytes (most significant first), channel u8 (Wi-Fi; 0 = hop) | RESULT |
| HUNT_STOP | `0x09` | 2 | none | RESULT |
| GET_HUNT | `0x0A` | 2 | none | HUNT_READING |

- **HUNT_READING** (type `0x8A`), one frame:
  - `readings u16`: sightings since the last GET_HUNT;
  - `latest i8`, `strongest i8`, `weakest i8`, and `mean i8`, all in dBm, over those readings;
  - `msSinceHeard u16`, saturating;
  - `channel u8`, and flags (locked on channel, target lost).
- **Pull, not push (rule 4) stays.** The interface polls GET_HUNT four to five times a second. One request is outstanding at a time, so nothing queues.
- **On the Hunter,** HUNT compares the source address of every Wi-Fi frame and Bluetooth advertisement with the target before classification:
  - Wi-Fi stays on the target's channel instead of hopping.
  - Bluetooth scanning runs continuously.
  - The address byte-order defect must be fixed in the same change, or Bluetooth hunts would never lock.

### Card ops (2026-11-04)

The phone pulls the Hunter's SD card files over the Link, so field analysis needs no
computer. Read only. Code: `libraries/LayerTime-Core/src/core/link/LinkCard.h`; tests: `test/test_link_card`.

- **CARD_LIST** (op `0x0B`, 2 bytes: op, reqId) answers one **CARD_ENTRY** (type `0x8B`,
  13 bytes: type, reqId, kind, serial u16, eventId u32, size u32) per file, then END.
- **CARD_READ** (op `0x0C`, 13 bytes: op, reqId, kind, serial u16, eventId u32, offset u32)
  answers **CARD_BYTES** (type `0x8C`: type, reqId, offset u32, bytes) from the offset to
  the end of the file at the connection's size, then **CARD_DONE** (type `0x8D`, 10 bytes:
  type, reqId, size u32, crc32 u32 of the whole file), then END. A dropped connection
  resumes from the last offset received.
- A file is a code, never text: kind 0 is `hunter_NNNN.log` (serial N, eventId 0); kind
  1 is `ev_NNNN/event_EEEEE.pcap` (serial N, event E). Only the Hunter's own files can be
  named.
- No card: RESULT NotReady. No such file, or a bad code: RESULT InvalidArgument. Wrong
  length: ERROR BadLength.
- HELLO_ACK capability bit 9 (`0x0200`): the Node serves its card.
- The Hunter sends a few frames per loop pass, so scanning and the Link keep running. One
  card read at a time, shared with the USB serial pull.
- **CARD_CLEAR** (op `0x0D`, 2 bytes: op, reqId; 2026-11-06) erases the card COMPLETELY:
  every file and folder, so a card full of tracker logs can be cleaned out in the field. It
  works a few files per loop pass, so scanning and the Link keep running. The Hunter then
  opens a fresh log, `hunter_0001.log`; records still waiting in its memory go there. It
  answers END (count = files erased, at most 255). No card, a card read running, or anything
  that would not erase: RESULT NotReady (the Hunter's serial log names what would not
  erase). Wrong length: ERROR BadLength. HELLO_ACK capability bit 10 (`0x0400`). LayerScope
  asks the operator to confirm, then sends CARD_LIST to show what the card now holds.
  Nothing on the phone is touched.
- Card file names (2026-11-06): logs are `hunter_NNNN.log`. Cards written before then hold
  `layerwand_NNNN.log`; the Hunter renames each one to `hunter_NNNN.log` (same number, so
  its `ev_NNNN` folder still matches) when it mounts the card. A name already taken is
  left as it is.

### Larger frames (Android only)

The Android watch can request a larger BLE MTU, and the Garmin cannot.
- **Mechanism:** a 0.2 HELLO may add one byte, `clientMaxFrame u8`. The Node answers with `maxFrame = min(clientMaxFrame, its own limit)`.
- **Garmin unaffected:** a 4-byte HELLO, which is the Garmin's, still means 20.
- **Effect:** at 20 bytes, a 256-byte Wi-Fi frame takes about 16 notifications. A larger frame cuts that, which matters for evidence pulls.
- **Phase 0 measures** the real throughput both ways.

### Vectors and review

- Every new frame gets byte-exact vectors in `contracts/vectors/link_frames.json`.
- **Compatibility gates:** both bindings pass the existing vectors unchanged, and the Garmin passes a real connection test against 0.2 firmware.
- This is a deliberate contract change, reviewed per `contracts/README.md`.

## Part 4: the watch

1. **Evidence.**
   - Opening a detection on a Node with rawCapture pulls its window, decodes every frame with Wireshark, and fills QUICK LOOK's investigation fields:
     - sender, receiver, and network;
     - frame type and reason code;
     - sequence-number jumps and retries, signs of forged frames;
     - the protected flag;
     - signal per frame, and the timing between frames.
   - The full Wireshark tree and hex sit below.
   - Export the window as a pcapng file.
2. **HUNT.**
   - A HUNT button on QUICK LOOK.
   - A full-screen gauge with the latest, strongest, and smoothed signal, and a tone that rises as the signal strengthens. It's usable without looking.
   - "Target lost" after a set silence.
3. **Bearing by manual sweep.** The watch has no compass or gyroscope.
   - Hold the Hunter against your chest, face each of eight directions, and tap once at each.
   - The watch takes the median HUNT signal over about two seconds per direction, and finds the weakest. The source is opposite, behind your body.
   - The arrow is relative to your starting direction, labeled plus or minus 45 degrees.
4. **Closest-approach walk-and-fix,** using the step detector.
   - Walk a straight line. The watch logs smoothed signal against step count.
   - The strongest point is where you passed closest, so the source lies roughly perpendicular to the path there.
   - A second line across the first resolves which side, and gives a rough fix. Step length is calibrated once.
   - GPS is used when it has a fix.
   - Shown as an area with its uncertainty, never a pin.
5. **Barometer floor hint.** If the signal peaks after a height change of about a floor, show "probably on this floor" as a hint.
6. **Optional later: a heading sensor on the Hunter.** A small compass and gyroscope board would let the sweep run itself and point to true north. It's a hardware decision.

## Phase plan

Each phase is reviewed before the next starts. Michael flashes and commits.

| Phase | What | Where | Gate |
|---|---|---|---|
| 0 | Measurement firmware (test build only). Wi-Fi length and FCS on the C5, RAM available for windows, BLE throughput at 20 bytes and with a larger MTU, frame rates under a deauth burst, and the HUNT poll rate. | Hunter test build | Numbers recorded; window sizes set from them |
| 1 | LayerTime-Sensors 1.2.0: the frame behind each match | Library | Host tests; API.md updated |
| 2 | Link 0.2 spec and vectors, both bindings | `contracts/`, C++ and Monkey C bindings | Existing vectors still pass; new vectors pass |
| 3 | Hunter: bootCount, evidence windows, evidence ops, HUNT ops, and the byte-order fix | Firmware | Host tests; on-device capture compared with TShark on the same bytes |
| 4 | Garmin compatibility | Garmin tactix against 0.2 firmware | Handshake, reconAvailable, GET_CHANGED, GET_TEXT, and recon alerts all still work |
| 5 | Watch: evidence decode, HUNT gauge, sweep, walk-and-fix, and floor hint | Android | Unit tests for the math; field test with a known source (your own AirTag) |

## Validation for the fox hunt

- **Known source, known spot.** Use your own AirTag and a known Wi-Fi access point.
- **Sweep:** check the arrow against the true bearing at several distances, and record the spread.
- **Walk-and-fix:** check the area against the true spot on two crossing lines, and record the miss distance.
- **Reporting:** publish the measured error ranges in the UI text, not guesses.

## Decisions (2026-10-30)

1. **Link 0.2 additions:** approved as drafted, including op numbers `0x06` to `0x0A`, capability bits 5 and 6, and Status flag bit 6.
2. **Phase 0 measurement firmware:** approved.
3. **Hunter Bluetooth byte-order defect:** fix it in Phase 3, with a test that proves the order.
4. **Heading sensor on the Hunter:** not decided; revisit after Phase 5 field results.
5. **Home of this design:** `contracts/link_0_2.md`.
