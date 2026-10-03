# LayerTime Link 0.1

Contract version 0.1, Slice 1 draft. The wire format between a LayerTime Node
(the T-Dongle-C5) and a LayerTime interface (the Garmin tactix) over BLE
GATT. The C++ binding is `src/core/link/`; the Monkey C binding is
`devices/garmin-tactix/source/link/`. Byte-exact vectors are in `vectors/link_frames.json`,
and both bindings are tested against them.

Slice 1 Increment 1 bound advertising, the service, Status, HELLO, PING,
link status values, the session rule, and the test-only range. The Recon
integration (2026-09-30) binds COMMAND/RESULT, GET_CHANGED/EVENT_SUMMARY/END
and GET_TEXT/TEXT, and gives Status its live Recon fields. RX_MARK stays
unbound. A Node that runs no Recon (the Increment 1 Link-only firmware)
still answers COMMAND, GET_CHANGED and GET_TEXT with ERROR UnknownOp, which
is how an interface tells the two apart.

## Rules

1. **Every frame is 20 bytes or less.** The Connect IQ BLE API writes at most
   20 bytes and does not negotiate a larger ATT MTU. Nothing here assumes a
   longer frame.
2. **One outstanding request.** The interface writes one Control request and
   waits for its reply on Data before writing the next. A reply is one frame,
   or for GET_CHANGED and GET_TEXT a run of frames that always ends with END.
   A request written before the previous reply has finished is answered
   with ERROR Busy.
3. **Integers are little-endian**, unsigned unless stated.
4. **Pull, not push.** The Node notifies Status; the interface fetches what it
   needs. Nothing can overflow a queue on either side.
5. **Timestamps never cross the link.** Only ages in seconds do.
6. **Session rule.** `sessionId` is random and non-zero at every Node
   power-on. Event identity is (`sessionId`, `eventId`). A changed `sessionId`
   means a new Recon session: the interface discards its mirror and tells the
   wearer.
7. **Failures are explicit.** Every request gets exactly one reply, and a
   rejected request gets an ERROR frame with a link status.

## UUIDs

One base UUID, allocated once for LayerTime, with the 16-bit field derived
per attribute. These are fixed.

| Attribute | UUID |
|---|---|
| Base | `8ac687f5-a08b-4851-b89f-e6b39081268e` |
| Service `LayerTime Link` | `8ac60001-a08b-4851-b89f-e6b39081268e` |
| Control characteristic | `8ac60002-a08b-4851-b89f-e6b39081268e` |
| Status characteristic | `8ac60003-a08b-4851-b89f-e6b39081268e` |
| Data characteristic | `8ac60004-a08b-4851-b89f-e6b39081268e` |
| Probe characteristic (test builds only) | `8ac600f0-a08b-4851-b89f-e6b39081268e` |

## Advertising

- The primary advertisement carries the flags AD and the 128-bit service UUID
  (21 bytes of the 31 available).
- The scan response carries the complete local name `LT-C5-xxxx`, where
  `xxxx` is the last two bytes of the Node's Bluetooth address in upper-case
  hex.
- The Node advertises whenever no central is connected, and accepts one
  central at a time. On disconnect it advertises again at once.
- The interface discovers by service UUID, not by name.

## Service

| Characteristic | Properties | Direction | Contents |
|---|---|---|---|
| Control | Write with response | interface to Node | One request per write |
| Status | Read, Notify | Node to interface | The 18-byte Status snapshot; read on connect, notified once a second, and within 200 ms of a change to any field other than heartbeat |
| Data | Notify | Node to interface | One reply frame per notification |
| Probe | Notify | Node to interface | Test builds only: payloads of 20, 21, 64 and 180 bytes for the notification-size measurement |

The interface enables notifications by writing `01 00` to the Client
Characteristic Configuration Descriptor (UUID `2902`) of Status, Data, and
(test builds) Probe.

## Versions

Link versions are `major.minor`. This document is Link 0.1. The Status
snapshot carries `linkVersion` as one byte, major in the high nibble and
minor in the low nibble (`0x01`). HELLO carries major and minor as separate
bytes. A Node rejects a HELLO whose major differs from its own with
`VersionMismatch`; a differing minor is accepted.

## Link status

| Value | Name | Meaning |
|---|---|---|
| 0 | Ok | Done |
| 1 | UnknownOp | The op byte is not defined, or is a test-only op on a release build |
| 2 | BadLength | The request length does not match the op |
| 3 | VersionMismatch | HELLO major differs from the Node's |
| 4 | Busy | A request arrived while another was still being answered |

Link status is about the link. `CommandResult` (`commands.md`) is about the
application and travels inside RESULT frames.

## Status snapshot (18 bytes)

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 1 | linkVersion | `0x01` |
| 1 | 2 | sessionId | random, non-zero, per power-on |
| 3 | 4 | changeSeq | starts at 1 each session; +1 whenever flags, selected, active, eventCount or lastAlertEventId change, or an event is created, changes, or is dropped; constant on a Link-only Node |
| 7 | 1 | flags | bit 0 monitoring, bit 1 earlyWarningEnabled, bit 2 earlyWarningResting, bit 3 alertPending, bit 4 sleepMode, bit 5 noSdLog (the Node keeps its events only in memory because no SD card log is running; the watch warns); 0 on a Link-only Node |
| 8 | 1 | selected | ReconTarget; 0 on a Link-only Node |
| 9 | 1 | active | ReconTarget; 0 on a Link-only Node |
| 10 | 1 | eventCount | events the Node holds now (at most 40); 0 on a Link-only Node |
| 11 | 4 | lastAlertEventId | eventId of the most recent event that raised an alert; 0 when none |
| 15 | 1 | heartbeat | +1 each second, wraps |
| 16 | 1 | schedule | 0 Simultaneous, 1 Recon, 2 Report; 0 |
| 17 | 1 | nextReportS | seconds to the next REPORT window, saturating; 0 |

The heartbeat is what the interface watches for link health. Under a
scheduled cycle (Increment 2B) `schedule` and `nextReportS` tell expected
silence from a lost link.

## Control requests

Every request starts `op u8 | reqId u8`. `reqId` is chosen by the interface
and echoed in the reply so replies can be matched.

| Op | Value | Total length | Arguments | Reply |
|---|---|---|---|---|
| HELLO | `0x01` | 4 | clientMajor u8, clientMinor u8 | HELLO_ACK |
| PING | `0x02` | 6 | token u32 | ACK |
| COMMAND | `0x03` | 3 or 4, per command | commandType u8, then that command's argument | RESULT |
| GET_CHANGED | `0x04` | 6 | sinceChangeSeq u32 | EVENT_SUMMARY..., END |
| GET_TEXT | `0x05` | 7 | eventId u32, field u8 | TEXT..., END |
| test-only | `0xF0`..`0xFE` | | RX_MARK is `0xF1`, not bound | on release builds: ERROR UnknownOp |

Any other op value is rejected with ERROR UnknownOp. A request whose length
does not match its op is rejected with ERROR BadLength, using the reqId at
byte 1 when the request is at least 2 bytes long and reqId 0 otherwise.

### COMMAND

`0x03 | reqId | commandType | argument`. `commandType` is the CommandType
number from `vectors/enums.json`. The Node accepts these, with exactly these
lengths:

| CommandType | Value | Length | Argument |
|---|---|---|---|
| ReconStart | 1 | 4 | target u8: a ReconTarget from 1 (All) to 16 (GoogleTag) |
| ReconStop | 2 | 3 | none |
| ReconClearEvents | 3 | 3 | none |
| ReconAcknowledgeAlert | 4 | 3 | none |
| SetSleepMode | 11 | 4 | enabled u8: 0 or 1 |
| SetEarlyWarning | 12 | 4 | enabled u8: 0 or 1 |

- A known command with any other length: ERROR BadLength.
- An argument out of range (target 0, which is ReconStop's job; a target
  above 16, EarlyWarning 17 included, which is SetEarlyWarning's job; an
  enabled value above 1): RESULT InvalidArgument, and nothing changes.
- Any other commandType, with a length from 3 to 20: RESULT Unsupported.
- A COMMAND shorter than 3 bytes: ERROR BadLength.

### GET_CHANGED

`0x04 | reqId | sinceChangeSeq u32`. The Node sends one EVENT_SUMMARY for
every event it holds whose own change sequence is greater than
`sinceChangeSeq`, oldest event first, then END. `sinceChangeSeq` 0 returns
every event. An event's change sequence is the Node's `changeSeq` at the
moment the event was created or last changed (a repeat sighting changes its
count, RSSI, channel, age and possibly confidence).

- The Node holds the 40 most recently created events. An interface that
  keeps a mirror keeps only the `eventCount` highest eventIds of the
  session; that removes events the Node dropped or cleared without a
  separate frame.
- END's `gap` is 1 when an event that changed after `sinceChangeSeq` has
  already been dropped, so the interface never saw that change.

### GET_TEXT

`0x05 | reqId | eventId u32 | field`, field 0 sourceId, 1 detail. The Node
sends the field's bytes in TEXT fragments of at most 14 bytes, then END with
`count` equal to the number of fragments. An unknown eventId, an unknown
field, or an empty field is answered with END and `count` 0. The bytes are
the text as the Node stores it, observed over the air, and so untrusted.

## Data frames

Every frame starts `type u8 | reqId u8 | linkStatus u8`.

### HELLO_ACK, type `0x81`, 10 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | type `0x81` |
| 1 | 1 | reqId |
| 2 | 1 | linkStatus: Ok or VersionMismatch |
| 3 | 1 | serverMajor |
| 4 | 1 | serverMinor |
| 5 | 2 | sessionId |
| 7 | 2 | capabilities: bit 0 localWifiMonitor, bit 1 localBleMonitor, bit 2 display, bit 3 led, bit 4 button |
| 9 | 1 | maxFrame: the largest frame this Node will send, 20 |

On VersionMismatch the Node still fills every field so the interface can
report both versions.

### ACK, type `0x82`, 8 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | type `0x82` |
| 1 | 1 | reqId |
| 2 | 1 | linkStatus Ok |
| 3 | 4 | token, echoed |
| 7 | 1 | heartbeat at the time of the reply |

### ERROR, type `0x8F`, 3 bytes

`0x8F | reqId | linkStatus`, for UnknownOp, BadLength and Busy.

### RESULT, type `0x83`, 5 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | type `0x83` |
| 1 | 1 | reqId |
| 2 | 1 | linkStatus Ok |
| 3 | 1 | commandType, echoed |
| 4 | 1 | CommandResult: 0 Ok, 1 Unsupported, 2 InvalidArgument, 3 NotReady, 4 Failed (`commands.md`) |

### EVENT_SUMMARY, type `0x84`, 18 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | type `0x84` |
| 1 | 1 | reqId |
| 2 | 1 | linkStatus Ok |
| 3 | 4 | eventId |
| 7 | 1 | detector (ReconTarget) |
| 8 | 1 | confidence |
| 9 | 1 | sourceKind |
| 10 | 1 | band |
| 11 | 1 | channel (0 when unknown or BLE) |
| 12 | 1 | rssi, signed (i8) |
| 13 | 2 | count, sightings including the first, saturating at 65535 |
| 15 | 2 | ageSeconds since last seen, saturating at 65535 |
| 17 | 1 | flags: bit 0 sourceId present, bit 1 detail present |

### END, type `0x85`, 9 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | type `0x85` |
| 1 | 1 | reqId |
| 2 | 1 | linkStatus Ok |
| 3 | 1 | count: frames sent before this END |
| 4 | 1 | gap: 0 or 1 (GET_CHANGED only; 0 for GET_TEXT) |
| 5 | 4 | changeSeq at the time of the reply |

### TEXT, type `0x86`, 7 to 20 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | type `0x86` |
| 1 | 1 | reqId |
| 2 | 1 | linkStatus Ok |
| 3 | 1 | field: 0 sourceId, 1 detail |
| 4 | 1 | index of this fragment, from 0 |
| 5 | 1 | total fragments |
| 6 | 1..14 | the fragment's bytes |

## Probe (test builds only)

The Node's test build adds the Probe characteristic. Each press of the
Node's button sends the next payload in the cycle 20, 21, 64, 180 bytes.
Byte 0 is the intended length modulo 256; byte `i` is `i` modulo 256. The
interface reports the length it actually received. Release builds do not
expose the characteristic, and `maxFrame` stays 20 regardless of what the
measurement shows until the contract says otherwise.

## Round-trip time

RTT is measured by the interface: the time from writing PING to receiving
the matching ACK, on the interface's own millisecond timer. The Node
records only the count of PINGs answered.
