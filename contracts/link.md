# LayerTime Link 0.1

Contract version 0.1, Slice 1 draft. The wire format between a LayerTime Node
(the T-Dongle-C5) and a LayerTime interface (the Garmin tactix) over BLE
GATT. The C++ binding is `src/core/link/`; the Monkey C binding is
`garmin/source/link/`. Byte-exact vectors are in `vectors/link_frames.json`,
and both bindings are tested against them.

This document binds what Slice 1 Increment 1 implements: advertising, the
service, Status, HELLO, PING, link status values, the session rule, and the
test-only range. The remaining operations and frames (COMMAND, GET_CHANGED,
GET_TEXT, RESULT, EVENT_SUMMARY, END, TEXT, RX_MARK) are defined by the Slice
1 plan and are bound here when their increments implement them.

## Rules

1. **Every frame is 20 bytes or less.** The Connect IQ BLE API writes at most
   20 bytes and does not negotiate a larger ATT MTU. Nothing here assumes a
   longer frame.
2. **One outstanding request.** The interface writes one Control request and
   waits for its reply on Data before writing the next.
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
| Status | Read, Notify | Node to interface | The 18-byte Status snapshot; read on connect, notified once a second and on every change |
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
application and travels inside RESULT frames from Increment 4 on.

## Status snapshot (18 bytes)

| Offset | Size | Field | Increment 1 value |
|---|---|---|---|
| 0 | 1 | linkVersion | `0x01` |
| 1 | 2 | sessionId | random, non-zero, per power-on |
| 3 | 4 | changeSeq | counts state and event changes; constant within a session until Increment 4 |
| 7 | 1 | flags | bit 0 monitoring, bit 1 earlyWarningEnabled, bit 2 earlyWarningResting, bit 3 alertPending, bit 4 sleepMode; 0 |
| 8 | 1 | selected | ReconTarget; 0 |
| 9 | 1 | active | ReconTarget; 0 |
| 10 | 1 | eventCount | 0 |
| 11 | 4 | lastAlertEventId | 0 |
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
| COMMAND | `0x03` | per command | bound in Increment 4 | RESULT |
| GET_CHANGED | `0x04` | 6 | bound in Increment 5 | EVENT_SUMMARY..., END |
| GET_TEXT | `0x05` | 7 | bound in Increment 5 | TEXT..., END |
| test-only | `0xF0`..`0xFE` | | RX_MARK is `0xF1`, bound in Increment 2B | on release builds: ERROR UnknownOp |

Any other op value is rejected with ERROR UnknownOp, and so is an op that
this document lists but whose increment has not bound it yet (so a Node built
at Increment 1 answers COMMAND with UnknownOp). A request whose length
does not match its op is rejected with ERROR BadLength, using the reqId at
byte 1 when the request is at least 2 bytes long and reqId 0 otherwise.

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

### Bound later

RESULT `0x83` (Increment 4); EVENT_SUMMARY `0x84`, END `0x85`, TEXT `0x86`
(Increment 5). Their layouts are in the Slice 1 plan, section 4.

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
