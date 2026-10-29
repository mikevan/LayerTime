# Wireshark decoding spike — actual results

Wireshark is the decoder. No substitute decoder was used. This records what was
actually built and run, separated from what is still planned.

## Pin

- **Host decode experiment:** Wireshark/TShark **3.6.2** (`Git v3.6.2 packaged as
  3.6.2-2`, Ubuntu 22.04), the version installable without root in the Linux
  build host. libwireshark `.so.15.0.2`, libwiretap `.so.12.0.2`, libwsutil
  `.so.13.1.0`.
- **Android target build:** pin a chosen stable source tag (current stable line
  is `v4.4.x`; newest tags seen upstream are `v4.7.x`). The 802.11 management
  dissection proven here is stable across these versions; the host proof is not
  version-sensitive for these fields.

## What actually ran (executed, not described)

Environment: the Cowork **device VM** (Ubuntu 22.04 x86_64), which — unlike the
cloud container — can reach the Ubuntu archive and github, but has **no root**,
**no NDK** (dl.google.com is blocked by egress), and no Android SDK mounted. So
the host decode ran here; the arm64 cross-compile cannot (see "Blocked").

Reproducible end-to-end by `reproduce_host_decode.sh` (no sudo): it fetches the
tshark 3.6.2 package closure with `apt-get download`, extracts the `.debs` into a
user prefix, builds the harness, and runs both decoders.

### Fixture

`make_fixture.py` emits `deauth_fixture.pcap` (DLT 105, raw 802.11, no radiotap —
exactly what a LayerWand promiscuous callback yields). The 26-byte frame:

```
c0 00 3a 01 ff ff ff ff ff ff 00 11 22 33 aa bb 00 11 22 33 aa bb 70 00 07 00
```

A deauthentication (type 0 / subtype 12), broadcast DA, SA/BSSID
`00:11:22:33:aa:bb`, seq 7, reason code 7 (class-3 frame from nonassociated STA).

### Decode 1 — TShark 3.6.2 (`tshark_decode.txt`)

```
frame.len  type_subtype  da                 sa                 bssid              seq  reason
26         0x000c        ff:ff:ff:ff:ff:ff  00:11:22:33:aa:bb  00:11:22:33:aa:bb  7    0x0007
```

### Decode 2 — libwireshark C embedding harness (`ws_harness.c`, `harness_decode.txt`)

The harness calls `epan` directly (`epan_init` → `epan_new` → `epan_dissect_run`
→ `proto_find_finfo`) on the raw bytes — the exact C path an Android JNI layer
would call, **not** the tshark CLI. Output:

```
wlan.fc.type_subtype   : 0x000c
wlan.da                : ff:ff:ff:ff:ff:ff
wlan.sa                : 00:11:22:33:aa:bb
wlan.bssid             : 00:11:22:33:aa:bb
wlan.seq               : 7
wlan.fixed.reason_code : 0x0007
```

### Comparison

Field-by-field identical between TShark and the libwireshark harness on the same
bytes. (`wlan.fc.type`/`wlan.fc.subtype` print `<absent>` from the harness
because they are hidden derived fields not added to the proto tree; the composite
`wlan.fc.type_subtype 0x000c` is authoritative and encodes type 0 / subtype 12.)

**Conclusion:** the libwireshark/epan C API decodes our target management frames
correctly and is embeddable through the same calls a JNI wrapper needs. This
is the decoder path for the Android detail screen.

## Measured sizes (actual, not estimated) — host x86_64 build

| Library | Size (stock Ubuntu build, already stripped) |
|---|---|
| `libwireshark.so.15.0.2` | **100 MB** |
| `libwiretap.so.12.0.2` | 624 KB |
| `libwsutil.so.13.1.0` | 200 KB |

`libwireshark` is ~100 MB because the stock build compiles and registers **all
~1,700 dissectors** at `-O2`. This is a measured host number, not an APK number,
but it sets the scale: a stock libwireshark per ABI is ~100 MB, so a stock
multi-ABI APK would be enormous. The reduction levers (still using Wireshark,
not substituting it) are: build only the dissectors on the 802.11 → LLC/EAPOL
handoff path, `-Os`, `-ffunction-sections -fdata-sections` with `--gc-sections`,
LTO, and one ABI (arm64-v8a) first. The arm64 build must report its own measured
stripped size before any size claim is made.

## Blocked here, needs the desktop (NDK)

The arm64 cross-compile could not run in any environment this session controls:
the cloud container has no NDK and its egress blocks `dl.google.com`, and the
device VM likewise cannot reach `dl.google.com` and has no root to install one.
The NDK is on the Windows desktop (Android Studio). Per plan, the arm64 build
runs there and this spike delivers the runnable block; its log comes back here.

## Next step

1. On the desktop: confirm the NDK path and the TCL K24's supported ABIs.
2. Generate the verified arm64 cross-compile block (GLib + deps + libwireshark,
   reduced dissector set, one ABI) and the JNI harness, run it, read the log,
   and record the **measured** arm64 `.so` size and decoded fields.

---

## Verified on the TCL K24 (2026-10-11): on-device Wireshark 4.4.19 decode

**Build:** 0.1.12-diag, versionCode 13, arm64-v8a. **Device:** TCL T434D, Android 14
(API 34), ABIs arm64-v8a, armeabi-v7a, armeabi. Source: photos of the diagnostic screen.

**Decoder:** libwireshark/epan **4.4.19**, cross-compiled with NDK r27c (target API 28),
loaded through JNI. No substitute decoder.

### Field comparison with same-version TShark (`tshark_4.4.19_decode.txt`)

| Field | TCL (epan 4.4.19, arm64) | TShark 4.4.19 (host) |
|---|---|---|
| length | 26 bytes | 26 |
| wlan.fc.type_subtype | 0x000c | 0x000c |
| wlan.da | ff:ff:ff:ff:ff:ff | ff:ff:ff:ff:ff:ff |
| wlan.sa | 00:11:22:33:aa:bb | 00:11:22:33:aa:bb |
| wlan.bssid | 00:11:22:33:aa:bb | 00:11:22:33:aa:bb |
| wlan.seq | 7 | 7 |
| wlan.fixed.reason_code | 0x0007 | 0x0007 |

All six fields match.

### Measurements on the TCL (build 13)

- APK on device 49,413 KB; native libraries installed 16 files, 129,089 KB
  (libwireshark.so 123,331 KB); installed total 178,502 KB. Build-host APK 50,589,602 bytes
  (build 12; build 13 not recorded).
- System.loadLibrary (ltws + 14 dependencies): 156.9 ms.
- Decode: first call 12.7 ms; warm median 6.3 ms (min 5.0, max 9.6, 20 runs).
- Native heap 47,428 KB -> 47,429 KB across 21 decodes. Process total PSS 263,422 KB
  (native PSS 28,591 KB); no pre-load baseline, so Wireshark's share was not separable.
- **Not valid:** the reported "epan_init: 0.1 ms". Registering ~1,700 dissectors cannot
  take 0.1 ms, and the fields decoded, so initialization had already run. The timer sat in
  Kotlin around a call that only initializes on first use. Working hypothesis (unverified):
  the report ran twice in one process (for example after unlock), and the second run showed
  an init that had already happened. Build 14 tests this directly.

### Build 12 failure and fix (for the record)

Build 12 failed to load: `dlopen failed: library "libintl.so" not found: needed by
libglib-2.0.so`. Android has no gettext, so GLib built its bundled proxy-libintl stub
(12 KB, needs only libc), which was not packaged. Fix: package libintl.so, and add
`check_native_closure.sh`, which fails the build unless every DT_NEEDED and every strong
undefined symbol resolves against the packaged set plus the API-34 platform libraries.

## Build 14 changes (decoder core and measurement integrity)

- Decode path moved into plain C (`app/src/main/cpp/ws_core.c`), with a thin JNI layer
  (`ws_jni.c`), so the host harness runs the exact code the APK runs.
- epan initialization timed inside native code (CLOCK_MONOTONIC); `epan_init`'s return
  value is checked; a later call reports `performed_now = 0` with the first call's timing
  and never re-runs initialization.
- Missing `frame_data_destroy` added after each decode (per-frame list and table).
  `epan_dissect_free` already releases the tvb chain (verified in epan/epan.c 4.4.19).
- Reports carry pid, report number, and activity instance; memory sampled before load.

### Host test of ws_core.c (x86_64, Wireshark 4.4.19), `host_core_test_100k.log`

- Init: first call performed init (epan_init 86.7 ms, OK); second call did not re-run it.
- Fields: all six match TShark 4.4.19.
- 100,000 decodes: 0 output mismatches; RSS 141,160 KB at 10,000 and at 100,000 decodes;
  malloc heap in use 16,229 -> 16,231 KB. No measurable growth. Mean 468 us per decode.

## Verified on the TCL K24 (2026-10-11): build 14 (0.1.13-diag, versionCode 14)

Full log: `tcl_build14_run.log` (UTF-16). Build-host APK 50,640,091 bytes; on device 49,453 KB;
native libraries 16 files, 129,091 KB; installed total 178,544 KB.

### Run A: fresh process (pid 24877, report #1, activity instance #1)
- System.loadLibrary (ltws + 14 dependencies): 162.9 ms.
- Native-timed initialization: **epan_init 864.8 ms**, returned true; total init 877.0 ms.
- Fields: 6 of 6 match TShark 4.4.19. Decode: first 7.5 ms; warm median 5.98 ms (min 5.40, max 6.98).
- Memory (total PSS / native heap): before load 81,557 / 11,952 KB; after load 162,383 / 12,005 KB;
  after init 220,636 / 41,800 KB; after 21 decodes 221,664 / 43,850 KB. Loading adds about 81 MB of
  PSS as mapped library pages (native PSS +0.1 MB); epan_init adds about 58 MB PSS and 30 MB native heap.
  Wireshark's total cost over the app baseline: about 140 MB PSS.

### Run B: same process, new activity instance (pid 24877, report #2, activity instance #2)
- Initialization reported "already done earlier in this process; not re-run" (init call #2), with
  report #1's timings. Load time likewise reported from report #1.
- This confirms the mechanism behind the invalid build-13 figure (epan_init 0.1 ms): a second report in
  one process shows an initialization that already happened. In the build-13 code the timed call was
  the first that could initialize, so 0.1 ms is only possible if an earlier report in the same process
  ran it. The build-13 run itself recorded no pid, so that specific run cannot be re-examined.
- Fields 6 of 6. Native heap 45,418 -> 45,419 KB across 21 decodes.

### Run C: fresh process, 20,000-decode stability test (pid 24975)
- epan_init 860.4 ms (true), total 862.1 ms; load 146.2 ms. Fields 6 of 6.
- **0 mismatched outputs in 20,000 decodes.** Mean 7,548 us per decode in the loop (includes result
  string creation and comparison; the 20-run warm median before the loop was 5,838 us).
- Samples (decodes: native heap / native PSS / total PSS / RSS, KB):
  2,000: 43,821 / 24,619 / 222,064 / 302,716; 4,000: 37,653 / 19,467 / 215,172 / 296,276;
  then rising evenly to 20,000: 37,657 / 19,441 / 219,520 / 300,744.
- Warm-up: between 2,000 and 4,000 decodes the native heap fell 6.2 MB (allocator returning memory).
  The in-app "growth from sample 1" figure (-6,164 KB) is therefore misleading; it starts inside warm-up.
- Steady state, 4,000 to 20,000 decodes: native heap +4 KB and native PSS flat: no native growth.
  **Total PSS +4,348 KB and RSS +4,468 KB, linear, about 270 bytes per decode, not plateaued at 151 s.**
  This growth is outside the native heap and is NOT yet attributed. It could be Java garbage awaiting
  collection (each decode returns a new string) or retained memory; the report has no Java heap
  breakdown and no forced collection, so the two cannot be told apart from this data.

## Build 15 (0.1.14): memory investigation, structured fields, PCAP import

### Decode path moved to shared C; host results (x86_64, Wireshark 4.4.19)
`ws_core.c` now also provides `ltws_control` (same alloc path, no dissection),
`ltws_decode_tree` (full protocol tree), and `ltws_decode_pcap`. Host harness
`ws_core_test.c`, log `host_core_decode_100k.log`:
- Structured tree: full Wireshark tree with byte offsets. The six key fields resolve at
  their correct offsets: wlan.fc.type_subtype @0 len1, wlan.sa @10 len6, wlan.da @4 len6,
  wlan.bssid @16 len6, wlan.fixed.reason_code @24 len2.
- PCAP import: deauth_fixture.pcap -> 1 frame, type_subtype 0x000c, sa/da, reason 0x0007.
- Decode loop, 100,000 iterations: 0 mismatches; RSS flat at 141,204 KB from 10k to 100k;
  malloc in use flat at 16,238 KB. Mean 499 us.
- Control loop, 100,000 iterations: 0 mismatches; RSS flat at 141,140 KB; heap flat at
  16,188 KB. Mean 0.4 us.
- On the host (no JVM), native heap and RSS do not grow for either loop over 100k
  iterations. This bounds the decoder's native retention at or below the sampling
  resolution; it does not by itself explain the Android PSS climb, which is measured
  next with per-category memory on the phone.

### What build 15 measures on the TCL
- Per-category memory each sample: Java heap, native heap, code, stack, graphics, other,
  system, total PSS, plus native-heap-allocated and RSS.
- A control mode (ltws_mode=control): same JNI byte[] in, String out, no dissection,
  to separate decoder cost from JNI marshalling and result-string allocation.
- A baseline taken AFTER warm-up (default 4000 iterations), so warm-up allocator return
  is not mistaken for the trend.
- Optional requested GC before each sample (ltws_gc=1): a diagnostic aid, not a guarantee.
  Normal runs (ltws_gc=0) are preserved so forced collection does not hide production
  behaviour.

## Run monitoring (from build 16 on)
Long phone runs report liveness so "working" can be told from "stalled":
- App: a `PHASE <name>` line at each phase change and a `HEARTBEAT` line at least every
  5 s inside long loops (iteration, interval rate, elapsed, ETA), flagged `SLOWING` when
  the interval rate drops below half the phase's first-interval rate. Shown live on screen.
- PowerShell run blocks stream those lines as they arrive, warn after 25 s with no app
  output while the process is alive (possible stall), detect a dead process and save the
  crash buffer, and enforce a per-run time limit. Logs are written as UTF-8.

## Build 15 and 16 results on the TCL K24 (2026-10-11)
Logs: `tcl_build15_run.log` (UTF-16, runs A, D, E, F) and `tcl_build16_run.log` (UTF-8).
Every run: device T434D, Android 14 (API 34), arm64-v8a; libwireshark/epan 4.4.19.

### Decoder output
- All 6 fixture fields match TShark 4.4.19 in every run (reference `tshark_4.4.19_decode.txt`).
- 0 mismatched outputs in every 20,000-iteration run (build 15 D and E, build 16).
- Structured tree: 72 fields with byte offsets; wlan.fc.type_subtype @0+1, wlan.da @4+6,
  wlan.sa @10+6, wlan.bssid @16+6, wlan.seq @22+2, wlan.fixed.reason_code @24+2.
- PCAP import of the fixture file: 1 frame, same fields.

### Timing (varies between launches; report as a range)
| | build 15 A | build 15 D | build 15 E | build 15 F | build 16 |
|---|---|---|---|---|---|
| System.loadLibrary | 160.6 ms | 150.3 ms | 145.6 ms | 148.3 ms | 385.8 ms |
| epan_init | 858.0 ms | 854.5 ms | 866.8 ms | 852.7 ms | 1,172.4 ms |
| warm decode, median of 20 | 6,102 us | 6,312 us | 6,137 us | 6,137 us | 6,283 us |
| mean iteration in the 20k loop | | 7,887 us | 7,887 us | 90 us (control) | 6,850 us |

### Memory, measured from the post-warm-up baseline over 20,000 iterations
| run | mode | GC before sample | Java heap | native heap PSS | nHeapAlloc | total PSS |
|---|---|---|---|---|---|---|
| build 15 D | decode | no | +5,636 KB, linear | -20 KB | +5 KB | +5,326 KB |
| build 15 E | decode | requested | +0 KB from 2k to 16k (10,256 KB each sample) | -16 KB to 16k | +1 KB | -189 KB to 16k |
| build 15 F | control (no dissection) | no | +5,836 KB, linear | -152 KB | -5 KB | +5,664 KB |
| build 16 | decode | no | about +570 KB per 2k before and after the RSS fall (+1,204 KB in the interval right after it) | see below | +7 KB | +4,444 KB |

What this establishes:
- The steady climb in total PSS is in the Java heap. The control loop (same JNI call and
  result string, no Wireshark dissection) climbs the same amount as the decode loop
  (+5,836 KB vs +5,636 KB), so the climb comes from the JNI round trip and the per-call
  result strings, not from the decoder.
- With a collection requested before each sample, the Java heap held at exactly 10,256 KB
  from 2,000 to 16,000 iterations. The climb is garbage the runtime can collect, not
  retained memory. GC was requested, not guaranteed; the normal runs are kept above.
- Native heap allocated changed by -5 to +7 KB over 20,000 iterations in every phone run
  (about 0.4 bytes per iteration at most). The host runs (no JVM) were flat over 100,000
  iterations. No native growth shows at this sampling resolution.
- Not established: absence of native leaks on other frame types, on the capture-import
  path, or over much longer runs.

### RSS falls (observed, cause not verified)
- Build 15 E, between 16,000 and 18,000: RSS -77,136 KB from baseline; java 10,256 -> 3,364 KB,
  native 19,452 -> 4,320 KB, code 174,564 -> 120,540 KB, sys 4,527 -> 80,170 KB; total PSS
  moved by only -1,441 KB.
- Build 16, between 6,000 and 8,000: RSS 297,304 -> 217,772 KB; java 12,160 -> 140 KB,
  native 19,432 -> 6,712 KB, code 174,128 -> 120,104 KB, sys 4,576 -> 84,035 KB; total PSS
  215,780 -> 215,283 KB.
- In both, the categories fell while "sys" rose by about the same amount and total PSS
  barely moved. One possible explanation is memory reclaim by Android (pages moved out
  of the app's resident set); this has not been verified. Per-category changes across
  such a point are not compared with the baseline. From build 17 the report prints an
  "observed" line whenever RSS falls by more than 20,000 KB between samples.

### Verdict wording (build 17)
Builds 15 and 16 printed "native heap grew" for any increase, including +5 and +7 KB of
noise. From build 17 the verdict reports growth only above a diagnostic threshold of
64 KB and 1 byte per iteration. This is a reporting threshold, not a leak test. The raw
figures are always printed.

### Run monitoring verified (build 16)
PHASE lines at load, init, decode, tree, pcap, warmup, stability, and done. HEARTBEAT every
5.0 s through the 26 s warm-up and the 137 s stability phase, 143 to 159 iterations/s, no
SLOWING flag, no stall warning, END reached; 171 s wall time for the whole block.

## Build 17 (0.1.16): capture import and decoded fields on the event-detail screen
- Native: `ltws_pcap_open / _next / _error / _file_type / _file_size / _close`, a
  frame-by-frame reader so the app reports progress and can cancel between frames.
  Each record set carries frame number, encapsulation, lengths, capture timestamp, file
  offset, the frame bytes (first 4,096) as hex, and the full Wireshark tree. Tree records
  gain a sixth column, Wireshark's label text for the node.
- Native: every dissection now holds one lock, so the diagnostic screen and an import
  never dissect at the same time (epan is not thread-safe).
- Each frame is dissected on its own; reassembly and conversation state across frames
  are not applied in this build.
- App: section 7 of the event-detail screen, IMPORTED CAPTURE (WIRESHARK), and the same
  section on its own via DECODE A CAPTURE FILE on the detected-attacks page (for use with
  no detection open, which is also how it is tested without live attack traffic). The user picks
  a file with the system picker; every frame is decoded and counted; the first 500 keep
  their full tree for display. Each frame expands to every field with its byte offset and
  the frame bytes. The screen states that the file is not linked to the event and that
  decoding a frame is not a "countered" claim.
- Progress: on screen (frames, share of file, rate, elapsed, slowing notice, cancel) and
  in logcat tag LTWS_PCAP (PHASE, HEARTBEAT every 5 s, first five frames' key fields, and
  a final PCAP DONE / CANCELLED / FAILED line).
- Test files (synthetic, not captured traffic) from `make_test_captures.py`:
  `lt_test_5frames.pcap` (deauth, disassociation, beacon SSID "LT-TEST" channel 6, probe
  request, deauth cut to 20 bytes) and `lt_test_bulk_3000.pcap` (the same five repeated).
  A unit test checks the five-frame file is byte-identical to what the app writes.
- The diagnostic report also runs the frame-by-frame reader on the five-frame file and
  checks frame 1 against the TShark 4.4.19 reference.

## Build 17 on three devices (2026-10-11 and 2026-10-28)
Logs: `tcl_build17_run.log`, `T513V_build17_stall_check.log`, `APPLLP5MAX_build17_run.log`.
Decoder output on all three: 6 of 6 fixture fields match TShark 4.4.19; the frame-by-frame reader
decodes all 5 test frames (deauth, disassociation, beacon with SSID, probe request, and the
truncated deauth marked MALFORMED); 0 mismatched outputs in every loop.

| device | chip | Android | RAM | decodes per second | warm decode median | epan_init |
|---|---|---|---|---|---|---|
| TCL K24 (T434D) | | 14 (API 34) | | about 150 | 6.2 ms | 858 to 1,172 ms |
| TCL T513V | MediaTek MT6835, 8 cores | 15 (API 35) | 3.6 GB | about 700 (until frozen, see below) | | |
| LOKMAT APPLLP 5 MAX (WP_C17S) | MediaTek MT6765 (Helio P35), 8 cores | 10 (API 29) | 6.0 GB | 200 to 260 | 3.8 ms | 534 ms |

### TCL T513V: the process was frozen by Android (verified)
20,000-decode run held about 700/s to 16,000, slowed to 499/s then 174/s, then went silent with the
process alive. The stall check showed `isFrozen=true`, `cached=true`, process state 16, both worker
threads parked in `do_freezer_trap` with 0 CPU over 5 s, and the process in the `background`
cgroup. Waking the screen did not thaw it because the app was no longer the front activity.
TCL's `tguard` was also killing cached apps in the same minute. Cause of leaving the foreground
not established (screen timeout, Home, or the system). Fix in build 18: capture import runs under
a foreground service; the diagnostic keeps the screen on and logs foreground changes.

### LOKMAT APPLLP 5 MAX
- Memory: about 308 MB resident with Wireshark loaded, 5% of RAM. Storage 109 GB free.
- Native heap allocated over 20,000 decodes: -101 to -135 KB from baseline through 6,000, a step
  to +301 KB at 8,000, then +113 to +271 KB, ending +115 KB. The last 4,000 decodes added 2 KB.
  Build 17's start-to-end verdict called this growth; build 18 adds the second-half trend so a
  single allocator step is told apart from a steady climb. Not settled: a 100,000-decode run is
  the next measurement.
- The watch clock was at the firmware build date (June 13) after the factory reset; set by hand.
- BLE scanning returned nothing until the system Location switch was turned on. Android 10
  returns no BLE scan results with Location off, whatever permissions the app holds
  (`WifiService: Location mode is disabled for the device` in the log). Build 18 checks the
  switch before scanning and tells the operator.
- Wi-Fi: MediaTek `gen4m` driver (`wlan_drv_gen4m.ko`, chip 0x6765). Monitor mode untested.
- Screen 640 x 480 at density 160. Build 17 layout: the CLEAR button was cut off in the
  detection row; everything else fit (screenshots in `screens_watch/`).

## Builds 18 to 20 (0.1.17 to 0.1.19): the watch layout
Build 18 verified on the LOKMAT watch (screenshots `screens_watch_b18/`): the face fills the
height at the left, slides off on a panel tap leaving the sliver, and slides back from the
sliver; CLEAR fully visible. Build 19 slid the panel with the face (rows started at the circle's
edge); rejected, the panel must not move. Build 20: the panel is a fixed layer laid out once
across the screen from the sliver; the face is clipped to a true circle on top of it (its own
black fill inside, transparent corners), so the panel shows through the square's corners and
the slide uncovers what the circle hid. Taps in the corners park the face like a panel tap.
The slide is translation only; nothing is re-measured.
- Watch layout: on landscape screens narrower than 1.6 x height, the face is a full-height
  square at the left over the panel. Tapping the panel strip or a face button slides it off
  left (260 ms) leaving a 40 dp sliver; tapping the sliver slides it back. While parked, the
  face's inputs are frozen so it is not redrawn each second; only the translation moves.
  Phones in portrait and wide landscape keep the earlier layouts.
- Detection buttons wrap (FlowRow) so CLEAR is always visible.
- Empty-list text reflects the real state (not connected / connected but stopped / running).
- "this phone" -> "this device".
- Location switch check before scanning on Android 11 and older, with an on-screen message.
- Capture import runs under `PcapImportService` (foreground, dataSync) with a progress
  notification and a Cancel action; the service stops itself when the import ends.
- Diagnostic: FLAG_KEEP_SCREEN_ON; `PHASE paused` / `PHASE resumed` lines on foreground changes.
- Verdict: "fell" wording for a drop; second-half least-squares trend separates a one-off
  allocator step from steady growth. Raw start-to-end figures are still printed.

## Build 21 (0.1.20): the steampunk rim
- Left of the line bisecting the circle vertically the face layer is black; right of it the
  face is transparent outside the circle, so the data panel shows through the right-hand corners.
- The rim is a 20/454 brushed-steel band cut into 14 uneven plates by dark seams, two rivets per
  seam, a mangled outer lip (dents up to 5/454 inward, never more than 2 px outward and never
  past the face's own circle), hammer dents, scratches, and rust on alternate plates. All vector,
  seeded with a fixed generator, so no bitmap memory and the same picture every frame.
- Host preview of the algorithm (Pillow port of the same drawing): `rim_preview.png`.

## Build 22 (0.1.21): worn metal inside the face, panel text scale, larger lone buttons
- Build 21 verified on the watch (`screens_watch_b21/`): rim, black left half, panel through
  the right-hand corners, slide and sliver all as designed.
- Inside the face every stroke keeps its width and gains a shadow stroke, a bright bevel
  hairline, seeded nicks and highlights, and rivets (ring diagonals, octagon corners).
- Panel text: the watch declares 160 dpi for both logical and physical density
  (`dumpsys display`: "density 160 (160.0 x 160.0) dpi"), so the true pitch is unknown to
  Android and a density-based fix is impossible. The panel scales its text from its own size
  instead: min(height/340, width/420) clamped to 1.0 to 1.8 (watch 596 x 464 dp -> 1.36;
  phone portrait -> 1.0), times a remembered A-/A+ multiplier (0.8 to 1.6) shown as
  TEXT SIZE at the bottom of the main panel. Buttons and padding follow the scale.
- Buttons that are a panel's only controls render at 44 dp x scale with 13 sp labels.
- Run blocks from here on write their log as they go and print a status line at least every
  15 s, including while waiting on the operator.

## Build 23 (0.1.22)
- Build 22 verified on the watch (`screens_watch_b22/`): text scale 1.4x, large lone buttons,
  worn strokes; the sub-pixel hairlines and 1.6/454 rivets on the octagons were too faint.
- COUNTER, RECON, and the NO LAYERWAND bracket are now drawn as weld beads: overlapping
  steel scallops ("stack of dimes") over a dark heat band, with a thin line of the element's
  own colour underneath so the cyan and amber still read. Outlines 8/454 longer. No rivets
  on welded parts.
- Hairlines and nicks are at least one device pixel; nicks darker.
- DETECTION EVENTS header 13 sp; TEXT SIZE A-/A+ moved directly under it so it stays
  reachable when the log is shown.

## Build 24 (0.1.23)
- The rim is drawn after everything else on the face, so COUNTER, RECON, and the bracket run
  under the bezel (a crystal held by the rim, above the metrics); a faint glass highlight
  arc sits just inside the bezel.
- Capture import never started on the TCL K24 (build 17) or the LOKMAT watch (build 23):
  the picker opened, no `PHASE copy` ever followed. Build 24 logs every step of that path
  under tag LTWS_PCAP: IMPORT tapped, picker launch failure if any, PICKER RESULT with the
  uri or "none", SERVICE start requested, SERVICE onStartCommand, IMPORT start requested.

## Build 24 result: capture import verified on the LOKMAT watch (2026-10-28)
Raw log `APPLLP5MAX_build24_rawlog.log`. The full chain ran: IMPORT tapped -> picker ->
PICKER RESULT (Downloads document uri) -> PcapImportService -> PHASE copy/init/decode ->
PCAP DONE. `lt_test_5frames.pcap`: 5 frames, 1 malformed, all key fields correct, 0.1 to 0.3 s.
`lt_test_bulk_3000.pcap`: 3000 frames, 500 kept, 600 malformed, 64.0 s at a steady 47 frames/s
(full-tree extraction per frame; the fixture-only diagnostic runs about 200/s on this chip).
First completed capture import on any device. Not yet tested: an import with the app sent to
the background mid-run.
Test-script defect: `adb logcat -d -s TAG:I TAG:E ...` returned nothing on this watch although
an unfiltered read shows the lines. All run blocks now read the log unfiltered and filter on
the PC.

## Build 25 (0.1.24)
- Slide sounds: synthesized pneumatic hiss sweeping down into a latch clank (slide out) and
  sweeping up into a heavier double latch (slide back); res/raw WAVs, 22 kHz mono, 23 and
  26 KB, played through SoundPool on the sonification usage. Generator parameters are in the
  session notes; no third-party audio.
- Grey text 0xFF8C8C8C -> 0xFFB4B4B4 app-wide.
- Shorter on-watch text for the capture-import blurb, screen-off test, export, Location
  message, and the per-frame note.

## Build 26 (0.1.25): the glowing console
- Console restyled to contrast with the bronze bezel: flat dark blue-black surfaces, one-pixel
  bright cyan edges with a smooth halo (four overlapping bands of falling alpha) and a
  cyan-tinted drop shadow, 10 dp rounded corners, glowing section headings. Buttons flat with
  green or cyan halos. Event rows amber-edged; frame cards green or red by state. Event-detail
  sections and the capture screen use the same frame. All draw-only; no layout cost.

## Build 27 (0.1.26)
Build 26 on the watch (`screens_watch_b26/`): edges visible, halo and tinted shadows too faint
to register, card surface nearly the console colour. Build 27: halo bands 0.55 to 0.05 over
five widths, an inner glow inside each edge, 1.5 dp edges, surfaces 0xFF13202C on 0xFF05090E,
heading glow blur 12, and halos on buttons.

## Build 28 (0.1.27)
Face content (battery arc, title, owl, rings, time, plates, bracket, date) laid out at 86% of
the circle's diameter and centred, as on the Garmin tactix face, so the bezel frames it with
air around it. The rim, the left-half mask, and the crystal sheen stay at full scale; the
COUNTER and RECON tap rectangles scale with the content.

## Build 29 (0.1.28): face re-layout to the Garmin rhythm
Build 28 (86% uniform scale) kept the old proportions: wide battery arc, small time below the
owl, rings high and apart, wide plates and bracket. Build 29 re-lays the content (454 box):
battery a 60-degree stub at the top; title 70; owl 78 at 120 with ear glyphs at 104; rings
radius 52 at y 212, centres 103 and 351; time 66 at 200 and 262 between the rings; plates
118 x 46 at 272 centred under the rings; bracket 200 wide, 328 to 382; date 406.

## Build 30 (0.1.29)
Build 29 on the watch matched the Garmin rhythm. Build 30: owl 90 at 122 with ear glyphs at
52 from centre, plates at 280, bracket 334 to 388, count text 19, date 410.

## Build 31 (0.1.30): Garmin ring content
- Ring text as on the Garmin face: small glyph and label in the ring colour, large white
  value, unit or AM/PM under it.
- SUNRISE / SUNSET: the next event computed on the device from the last known location
  (USNO Almanac for Computers method, zenith 90.833; unit-tested against Washington DC
  2026-10-29, Quito at the equinox, and Svalbard polar night). No location -> "SUN --".
  Phones on Android 12+ do not hold location permission, so they show "--".
- TEMP: only from a real ambient temperature sensor (TYPE_AMBIENT_TEMPERATURE); with none,
  N/A. Battery or CPU heat is never shown as air temperature. Fahrenheit for US locales.
- Log tag LT_RINGS reports whether the sensor and a location fix exist.

## Build 32 (0.1.31)
Ring stroke 7 -> 9.5, battery stub 9 -> 12, weld bead radius 1.7 -> 2.3 with a wider heat band
and colour line; owl 90 -> 106 at 126, ear glyphs moved out to 66 from centre.

## Build 33 (0.1.32)
Face: content box is the full circle and every element is placed so its outer edge sits at
radius 201 of 227, just inside the bezel's inner edge (205): battery stub radius 193.5,
rings centred 143 from the centre at y 222, plates at x 55 and 281 (y 290 to 336), bracket
278 wide with 31 slant (y 342 to 394). Owl 100 at 134; time 64 at 212 and 272; date 414.
All strokes inside the circle +25% (rings 11.9, battery 15, welds 8.1 band / 2.9 beads,
dashes 2.5, wear lines 1.0 to 1.1, glyphs 2.5 px).
Console: panel and button edges are tube-shaded (dark outer and inner edges, edge colour,
bright core), 3.6 dp; divider rails 3 dp with vertical shading; primary buttons domed.
Fonts: SIL OFL fonts bundled (Orbitron, Rajdhani, Exo 2, Share Tech Mono; licences in
assets/licenses), taken from the Fontsource npm packages and converted WOFF2 -> TTF with
fontTools. FONT button cycles: SYSTEM, ORBITRON + RAJDHANI (default), ORBITRON, EXO 2,
RAJDHANI; the choice is remembered.

## Build 34 (0.1.33)
Face: laid out on the exact coordinates of devices/garmin-tactix/source/HomeView.mc, scaled
by 0.93 about the centre so the battery arc sits inside the bezel. Text is drawn by ink like
the Garmin fonts (capital height and bottom row): XTINY 19 px capitals, TINY 27, SMALL 31 px
digits, NUMBER_MEDIUM 59 px digits. Build 33 sized text by font size, so capitals came out at
about 60% of the Garmin. Battery: 120 degree arc at r 209, ticks every 4 degrees, icon plus
BAT n% on rows 42..60. Title on 68..86. Owl 100 centred at 141, ear icons at +/-72, y 118.
Hours ink 192..251, minutes 258..317. Rings R62 at y 190, x 95 and 359, with bands at y-24..-6,
0..31, and 36..54. Buttons [22,262,133,63] and [299,262,133,63]. Recon brackets 326..393, at
least 220 wide. Date on 401..419. Strokes are the Garmin's pens times 1.7.
Console: HUD style. Top status strip (link state, event count, sweeping scan glow); panels
with a gradient surface, a faint grid, and glowing corner brackets; title tabs with a
pulsing LED, slash accents, and a tick-marked rail; pill buttons (glass or domed, with a
specular line); disabled buttons stay visible; recessed readouts for counts. Shadows on
frames and buttons no longer clip their own halo (clip = false).

## Build 35 (0.1.34)
Console borders doubled: the tube-shaded edge around the console, its panels, and its cards
is 7.2 dp (was 3.6, now the FrameEdge constant in Theme.kt); corner brackets 6 dp (was 3).
Inner padding grown by about 4 dp so content clears the thicker edge.

## Build 36 (0.1.35)
Console borders doubled again: FrameEdge 14.4 dp, corner brackets 12 dp wide and 32 dp long.
Corner radii grown so the thick edge stays rounded (console 20, panels 18, cards 16 dp), and
inner padding grown by about 7 dp to clear the edge.

## Build 37 (0.1.36)
Warm circle, cold console. The dial behind the face metrics is a very faint burnished bronze
(radial #2B1D10 lit from the upper left to #0E0905 at the bezel) with fine seeded concentric
polishing arcs and a soft sheen; the steel bezel gets a translucent bronze glaze (#B87333 at
18%). Console colder: background #02060F, surface #0B1A2C, edge #6FE6FF, grid lines ice-blue
at 10%.

## Build 38 (0.1.37)
Bronze one step stronger: dial #3C2916 -> #281B0E -> #140D07, polishing arcs and sheen about
60% stronger, bezel glaze 25% (was 18%).

## Build 39 (0.1.38)
Rings. Diagnosis on the watch (b38_rings_diag.txt): Android 10, location granted, mode 3,
network location disabled and no active network, no last known location at all; an MTK
AMBIENT_TEMPERATURE sensor exists (on-change) and was registered but never delivered a
reading. Fixes: the sensor's readings are logged (first three, then one a minute, and a line
if nothing arrives in 10 s), and TEMP shows "--" while waiting (N/A only with no sensor).
Location: asked for when the app opens; GPS (and network when enabled) is retried, 3 minutes
on and 5 off, until a fix, then refreshed every 30 minutes; the last fix is saved in prefs so a
restart keeps it; the ring reads NO FIX while searching and NO LOC without permission.
ACCESS_FINE_LOCATION is no longer capped at API 30, and COARSE is declared.
The watch layout rule is now WatchFormFactor (unit tested), tied to the design standard.

## Build 40 (0.1.39)
Evidence from b39_hw_diag.txt. TEMP: sensorservice's recent-events record has STEP_COUNTER,
LIGHT, and ACCELEROMETER but never AMBIENT_TEMPERATURE, so the listed MTK ambient sensor has
never produced a reading; the product listings name no temperature sensor. GPS: the MediaTek
GNSS stack is real (MNLD hal_gps_start, GNSS HAL wakelock, MtkAgpsNative), but assisted GPS
fails with no SIM ("IMSI is invalid") and no cell data, so every start is an unassisted cold
start; no fix arrived in 2 minutes.
Changes: a temperature sensor silent for 10 s is treated as absent and TEMP shows N/A. The
receiver keeps searching (up to an hour per attempt) until the first fix, then refreshes for
3 minutes every 30. GnssStatus is logged every 30 s (satellites used, in view, best C/N0), and
the SUN ring's bottom line shows SAT used/seen while there is no position.

## Build 41 (0.1.40)
GPS evidence (b40_rings_log.txt): the receiver works (22 satellites in view within 30 s) but
the strongest signal was 21 to 23 dB-Hz, an indoor level, then 0; no fix. A later outdoor try
also gave no fix (log not yet collected). The LayerWand (T-Dongle-C5) has no GNSS, and the
watch cannot pair with a phone, so the watch's sources are its own GPS and manual entry.
Manual position: a POSITION panel in the console takes MGRS (any precision, spaced or not),
UTM, or decimal latitude and longitude (with N/S/E/W or signs), converts on the watch
(GridPosition: Snyder transverse Mercator series on WGS84, MGRS AA lettering, band-resolved
northing, Norway and Svalbard zone exceptions; polar areas refused), and saves it. The SUN
ring uses whichever is newer, the last GPS fix or the manual entry, and marks a manual
position MANUAL in amber. GridPositionTest checks 9 reference points from the Python mgrs
package and PROJ (both hemispheres, equator, 32V, 33X) to 1 m and 0.5 m, plus parsing and
refusals.

## Build 42 (0.1.41)
Outdoor GPS (b40_outdoor_log.txt, 13:17 to 14:04): at most 1 satellite in view, mostly 0,
never above 24.4 dB-Hz, no fix; worse than at the desk. This watch's GNSS reception is not
usable, so manual entry is its primary position source.
Position entry: on the watch the system keyboard would not accept the digits after "18S UJ".
The POSITION panel now has its own HUD keypad (digits, minus, decimal point, ABC for the 24
grid letters without I and O, SPACE, DEL, CLEAR) typing into a readout well; the system
keyboard is not used.

## Build 43 (0.1.42)
"PM MANUAL" and "SAT 0/0" on the SUN ring's bottom row ran outside the ring: that row's chord
is only about 60 px. The bottom row now carries only AM or PM (held to 56 px), and MANUAL, or
the satellite count while there is no position, sits on a small riveted plate (dark bronze,
coloured rim, held to 80 px) in place of the sun glyph at the top of the ring, where the
chord is about 84 px.
