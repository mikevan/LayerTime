# Android Wireshark (libwireshark/epan) build feasibility

Scope: can `libwireshark`/`epan` be cross-compiled for Android via the NDK and
linked into the controller APK to decode 802.11 frames? This reports **measured
facts and concrete blockers**, kept separate from planned work. It does not
substitute another decoder; the recommendation at the end is for your decision.

> **Correction / update.** An earlier draft here implied Android cross-compile
> was unsupported because Wireshark ships no `android.cmake`. That inference was
> wrong: the **NDK provides the standard `android.toolchain.cmake`**, which is
> how any CMake project targets Android — a project does not need to bundle its
> own. Wireshark shipping none is not a blocker. Separately, the decode question
> is now **answered with a real build**: libwireshark 3.6.2 decodes our deauth
> fixture, verified against same-version TShark — see
> `wireshark-spike/RESULTS.md`. The size figure below is now a **measured** host
> number, not an estimate.

## Status summary

- **Decoding results today: none, because there is nothing to decode.** Per
  `FRAME_EVIDENCE_PROPOSAL.md`, LayerTime Link 0.1 carries zero raw 802.11
  bytes to Android. Any decoder — Wireshark or otherwise — has no input until a
  Link 0.2 raw-frame transfer path exists and is approved. The decoder choice is
  therefore being settled *now*, ahead of that data, exactly as requested.
- **I did not complete an NDK cross-compile of libwireshark in this session**,
  and state plainly why below rather than report a build I did not run.

## What I actually measured (Wireshark `master`, fetched file-by-file)

Source inspected directly from the upstream mirror
(`raw.githubusercontent.com/wireshark/wireshark/master`):

| Fact | Evidence |
|---|---|
| GLib ≥ 2.54 is a hard, required dependency of the whole build | `CMakeLists.txt:1511` — `find_package(GLIB2 "2.54.0" REQUIRED)` |
| No Android/NDK build target ships with Wireshark | only `android` hits in the top CMake are `androiddump` (`:2412`), a **host-side** USB capture helper, not a cross-compile target; Wireshark uses the NDK's own `android.toolchain.cmake` instead |
| epan is a whole-framework static-registration library | `epan/dissectors/CMakeLists.txt` lists **1,717** `packet-*.c` dissectors compiled and registered together |
| The 802.11 dissector alone is very large | `epan/dissectors/packet-ieee80211.c` = **2,463,248 bytes / 64,074 lines** |
| epan core headers are GLib/wmem-coupled throughout | `epan/proto.h` (177 KB) uses 26 GLib-typed symbols and 10 `wmem_` (GLib-backed allocator) references; `packet.h`/`proto.h`/`tvbuff.h` ≈ 408 KB of headers |

## Why no build was run here (environment blockers, concrete)

1. **The NDK is not reachable from any environment this session can build in.**
   The Android SDK/NDK lives on the desktop at `%LOCALAPPDATA%\Android\Sdk`; the
   device bridge mounts only `C:\workspace\TUltra-Project\LayerTime`, so the NDK
   is not visible to the device shell, and the cloud container has no NDK.
2. **The cloud container cannot clone the Wireshark source.** The egress proxy
   denies `gitlab.com` (HTTP 403 on the CONNECT). Only single-file reads from
   `raw.githubusercontent.com` succeed — enough to measure the source, not
   enough to drive a CMake configure/build.

A real attempt therefore belongs on the desktop and is a multi-day spike, not a
quick build — see the chain below.

## What a real libwireshark-on-Android build actually requires

Because GLib is `REQUIRED` and Wireshark ships no Android toolchain, the chain
per ABI (arm64-v8a at minimum, likely also armeabi-v7a / x86_64) is:

1. Cross-compile **GLib ≥2.54** with the NDK, which itself needs **libffi**,
   **PCRE2**, and **zlib/iconv** cross-compiled first. GLib assumes a desktop
   libc; its Meson build has no first-class Android target and needs patching.
2. Configure Wireshark's CMake with the **NDK's `android.toolchain.cmake`** (the
   standard NDK-provided toolchain file; Wireshark ships none of its own but does
   not need to) and disable the large surface it cannot use on Android (Qt UI,
   dumpcap/extcap, Lua, SNMP/MIBs, Kerberos), building `libwireshark` +
   `libwiretap` + `libwsutil` only.
3. Either accept the **whole-framework static registration** (all 1,717
   dissectors, because epan's generated `register.c` and the dissector handoff
   graph expect them) or hand-prune — the 802.11 dissector hands off to LLC/SNAP,
   EAPOL, IP and more, so "just 802.11" is not a supported configuration.
4. JNI glue from Kotlin to `epan` init / `dissect_packet` / `proto_tree`.

**Size reality (now measured on a real build, host x86_64):** the stock
`libwireshark.so` is **~100 MB** (already stripped — all ~1,700 dissectors at
`-O2`), with `libwiretap` ~624 KB and `libwsutil` ~200 KB (see
`wireshark-spike/RESULTS.md`). That is a host number, not an arm64 APK number,
but it sets the scale: stock, one ABI, libwireshark alone is ~100 MB. Reduction
levers that keep Wireshark as the decoder — a reduced dissector set on the
802.11 → LLC/EAPOL path, `-Os`, `--gc-sections`, LTO, single ABI — must be
applied and the arm64 `.so` **measured** before any APK-size claim.

## Recommendation (for your call — no decoder is being substituted)

For the subset the use cases need — deauth, disassociation, beacon,
auth/assoc — the decodable content is a **fixed-layout 802.11 management
header**: frame-control type/subtype/flags, duration, the three/four MAC
addresses, sequence/fragment, the 2-byte reason/status code, and a short TLV
walk of information elements (SSID, supported rates, RSN). This is a few hundred
lines of pure Kotlin with no native build, no GLib, no licensing drag, and it
runs on both target phones unchanged.

- **Recommended first step:** implement that focused native-Kotlin management
  dissector and validate it **field-by-field against TShark** on the same
  captured bytes (the validation plan in `FRAME_EVIDENCE_PROPOSAL.md` §D).
- **Keep libwireshark/epan as the evaluated fallback** if a full protocol tree
  for arbitrary frame types later becomes a hard requirement; at that point the
  desktop spike above is the path, with a measured size before commitment.

This is the recommendation, not a decision. I will not swap in a different
decoder without your approval; if you want the libwireshark path attempted
despite the cost, say so and I will scope the desktop spike.
