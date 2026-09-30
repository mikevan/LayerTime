# LayerTime Connect IQ Device App

The Garmin side of LayerTime Slice 1: a foreground Connect IQ Device App for
the tactix 8 AMOLED (Connect IQ device id `fenix847mm`, which also covers the
fēnix 8 and quatix 8 47/51 mm). Increment 1 adds the LayerTime Link client
(`source/link/`): the app discovers the C5 by service UUID, pairs, enables
notifications, shows the Status heartbeat and session, and sends PING on
START with the round-trip time shown. `linktest.jungle` builds the same app
with the test-only 100-PING burst (`source/link/LinkBurst.mc`, UP starts
it) for the Increment 1 acceptance measurement; `monkey.jungle`, the release
build, excludes it. The burst logs every round trip to
`GARMIN/Apps/Logs/LayerTime.TXT` when that file exists on the watch. The same
build has the 10-minute Status stability measurement
(`source/link/HeartbeatWatch.mc`, DOWN starts it while Connected, a second
DOWN is ignored, it ends itself at 600 s from the first Status received):
PASS needs the full window, no Status gap over 2000 ms, and no disconnect;
it logs `hb,start`, `hb,gap,<n>,<ms>` for gaps over 2000 ms,
`hb,disconnect,<count>`, and `hb,end,PASS|FAIL,...` with duration, received,
intervals, min, mean, p95, max, gaps, and disconnects. `source/link/LinkVectors.mc` is
generated from `contracts/vectors/link_frames.json` by
`tools/gen_link_vectors_mc.py`; do not edit it by hand.

Requires Connect IQ SDK 9.2.0 or later and the Monkey C extension for
VS Code. Open this `garmin/` folder as the workspace root, or add it as a
folder, so the extension sees `manifest.xml`.

## Build and run in the simulator

1. Open the command palette and choose **Monkey C: Build Current Project**,
   then pick **fenix847mm** as the product.
2. Choose **Run > Start Debugging**. The simulator opens with LayerTime.

## Install on the watch

Choose **Monkey C: Build for Device**, pick **fenix847mm**, and copy the
resulting `.prg` from `bin/` into the watch's `GARMIN/Apps/` folder over USB.

## Unit tests

The tests in `test/` use the SDK's Run No Evil framework. In VS Code, open
the Test Explorer (the flask icon) and run them, or from the command line
build with `--unit-test` and run `monkeydo <prg> fenix847mm /t` with the
simulator already open.

## Verified platform findings

Facts established on real hardware, with the evidence, so they never turn
into folklore. Each one binds later work.

### F1. No BLE transaction from inside a BLE callback (2026-09-30)

**Hardware and runtime.** tactix 7 AMOLED, M/N A04597, part 006-B4542-00,
firmware 27.18, Connect IQ 6.0.2 (Connect IQ device id `epix2pro51mm`;
the SDK 9.2.0 device file still lists it at Connect IQ 5.2.0, the watch is
newer). Build: linktest.jungle for `epix2pro51mm`, `minApiLevel` 5.0.0.

**What happened.** The Increment 1 PING burst issued the next PING from
inside the callback that delivered the previous ACK. The watch raised an
unhandled exception at the Control `requestWrite`. Symbolicated from the
crashing PRG and its matching debug XML, top to bottom:

```
LinkClient.mc:336 sendNext          (Characteristic.requestWrite)
LinkClient.mc:319 enqueue
LinkClient.mc:148 ping
LinkBurst.mc:74   sendOne
LinkBurst.mc:68   onPing
LinkClient.mc:405 notifyPing
LinkClient.mc:401 applyReply
LinkClient.mc:235 onCharacteristicChanged
```

The same production code path that issues follow-on requests from
`onDescriptorWrite` and `onCharacteristicRead` (the CCCD, Status read, and
HELLO sequence) works on this watch; the failing case is a new
`requestWrite` started from `onCharacteristicChanged`, the notification that
carried the reply to the previous write, before the system had delivered
that write's own `onCharacteristicWrite`.

**Rule.** A BLE reply or notification callback (`onCharacteristicChanged`,
and by extension every `BleDelegate` callback) may update state and record
results. Any follow-on BLE transaction is deferred until control has
returned to the Connect IQ event loop: schedule it with a one-shot
`Timer.Timer` (50 ms, the documented host minimum) or from the app's own
tick, and only once the previous write's completion has been delivered
(`LinkClient.writeInFlight` is false). One request stays outstanding at any
time. `source/link/LinkBurst.mc` is the reference implementation of the
rule; `LinkClient.pingObserver` is called inside the callback and must obey
it.

**Status.** Validated 2026-09-30: the corrected burst ran 100 PINGs, 100
ACKs, 0 lost on the watch, and the 10-minute Status measurement passed.

### F2. A GATT request slot is freed only by the stack's completion callback (2026-09-30)

**Hardware and runtime.** Same watch and firmware as F1 (tactix 7 AMOLED,
part 006-B4542-00, firmware 27.18, Connect IQ 6.0.2, `epix2pro51mm`),
linktest build, C5 on `tdongle_c5_test`, C5 powered throughout.

**What happened.** Acceptance item (d), walking out of range and back. The
system reconnected the paired C5 several times on the way out (`Reconnects
4`); on the way back the app died with an unhandled exception. Symbolicated
against the matching `garmin.prg.debug.xml`:

```
pc 0x10002038  LinkClient.mc:342  sendNext   descriptor.requestWrite([0x01, 0x00]b)   (a CCCD write)
pc 0x100018b5  LinkClient.mc:262  onTick     sendNext(_queue.complete(now))           (the 3 s request timeout)
```

Every reconnect re-runs the setup sequence (three CCCD writes, a Status
read, HELLO). On a marginal link the first CCCD write got no
`onDescriptorWrite` within our 3 s timeout, so `onTick` completed it and
issued the next CCCD write; the stack refused it. Whether the stack still
held the first request or had already dropped the link (with the
`DISCONNECTED` callback not yet delivered) cannot be told from the stack
trace, and does not matter: in both cases our timer, not the stack, had
declared the slot free.

**Rule.** A GATT request slot is freed only by the Connect IQ stack's own
completion callback (`onDescriptorWrite`, `onCharacteristicRead`,
`onCharacteristicWrite`) or by `DISCONNECTED`. An application timeout may
account for a request failure, but it must never itself grant permission
to issue another GATT request. F1 is the ordering half of the same rule
(do not issue before the callback); F2 is the timeout half (do not issue
instead of waiting for it).

**Correction A (production `LinkClient`, `RequestQueue`).**
`LinkClient.requestPending` is true from the moment any request (CCCD
write, Status read, Control write) is handed to the stack until that
request's completion callback or a disconnect; nothing is issued while it
is true. `RequestQueue.expire()` now reports a timed-out item once and
leaves it in flight; `complete()` alone frees it. The 3 s timeout keeps its
accounting (error text, a PING counted lost) and completes the item only
when the stack has already answered (for example a PING whose write
succeeded but whose ACK never came); otherwise the late completion callback
completes it. A request the queue releases while the stack is still busy
(a reply arriving before the previous write's own callback, the F1 case)
waits in `_deferred` and is issued from the callback. A completion callback
for an already-completed request is recognised and ignored.

**Observed alongside, and correction B.** After relaunching, the app
connected once, lost the link beside the C5, unpaired after 5 s, started
scanning, and sat in `Scanning` for 266 s next to an advertising C5. The
client had no scan watchdog. Correction B: while `Scanning` with no
matching result for 15 s, the scan is stopped and started again, counted
in `LinkClient.scanRestarts`, shown on the view as `Scan restarts N`, and
noted in the error line as `Scan restarted (N)`, so a recovery is
distinguishable from an uninterrupted scan. A and B are independent: A is
the crash, B is the recovery.

**Status.** Built (release and linktest, `-l 3`); awaiting the physical
regression: three genuine out-of-range cycles with recovery each time, no
crash, no `Scanning` beside the C5 beyond the watchdog window, then 10
minutes stable.
