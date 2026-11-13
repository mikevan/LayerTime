# Product and shared-library separation

Owner terminology confirmed November 13, 2026:

- **Vixen** is the flagship Android analysis product previously called LayerScope.
- **Hunter** is the LilyGO T-Dongle-C5 with sensors, SD logging, evidence capture and card tooling. Its existing logo is retained.
- The old fox-hunt beacon is a separate legacy build. Its old `tdongle_c5_vixen` environment and macro are compatibility names, not the Vixen flagship.
- **Notella** remains a separate training product and its reusable core remains independent of the LayerTime profile.

## Local repositories

| Repository | Prepared location | Ownership |
|---|---|---|
| Vixen | `C:\workspace\Vixen` | Android app, analysis, product-specific AI workflow and tracked teaching/evaluation assets |
| Hunter | `C:\workspace\Hunter` | C5 sensor application, BLE transport, SD logs, PCAP evidence, card transfer and diagnostics |
| LayerTime-Core | `C:\workspace\LayerTime-Core` | Hardware-independent C++ model, application logic, ports and Link codec |
| LayerTime-C5 | `C:\workspace\LayerTime-C5` | Shared C5 pins, display, LED, logo rendering, button/identity logic and SPI boot precaution |
| LayerTime-Sensors | `C:\workspace\LayerTime-Sensors` | Snapshot of the existing detector library at the revision LayerTime actually uses |
| LayerTime-Contracts | `C:\workspace\LayerTime-Contracts` | Language-independent wire/model definitions and compatibility vectors |

These were initially prepared as local repositories. The subsequent GitHub
checkpoints and remote URLs are recorded in `publication.json`. At the owner's
request, all seven LayerTime-related repositories are public. The five newly
created repositories were changed from private to public on November 13, 2026.
Each records its original extraction ancestry
and detector revision in `EXTRACTION.json`; that file is a historical preparation
receipt rather than live publication state.
Original Git history is retained in the original LayerTime repository; it has not
been rewritten or deleted. The top-level Sensors snapshot is the recorded
`35e7213874d4577bd9385f226b20e614a65fbfa8`, rather than the newer standalone
Sensors working directory elsewhere under TUltra-Project.

The original LayerTime checkout remains the watch/integration workspace. It now
builds the extracted packages under `libraries/` instead of compiling core as
application source. The top-level product repositories use the same extracted
library content as sibling dependencies. These are migration copies, not a
background synchronization service: make subsequent library changes deliberately
and update consumers' recorded dependency inputs together.

## Actual shared code

`LayerTime-Core` contains 64 relocated source files. Its public `core/...`
include paths remain stable. Hunter and both watch firmware configurations now
declare it as a PlatformIO library, and a CMake build is available independently
of Arduino or a device application. Detector headers remain public imports from
LayerTime-Sensors; the sensor submodule was not edited.

`LayerTime-C5` contains nine relocated board-support files and the small shared
`C5Board.h` boot helper. Hunter's SD logger delegates the chip-select precaution
to that helper. The beacon uses the same helper directly, so it no longer needs
the Hunter SD logger just to keep a card deselected at boot. SD logs, evidence,
card-transfer logic and radio acquisition remain Hunter product code.

Vixen's Kotlin Link implementation and Garmin's Monkey C implementation are
language bindings, not consumers of the C++ application library. They must agree
with the same Contracts definitions and vectors. No claim is made that Kotlin,
Monkey C and C++ are one compiled library.

## Dependency and build checks

The top-level Hunter build uses sibling Core, C5 and Sensors repositories.
`dependencies.lock.json` records every source input and package manifest by
SHA-256, with text line endings normalized for Windows/Linux compatibility.
`tools/dependencies_check.py` runs before a PlatformIO build and refuses missing,
changed or extra source files. It never silently updates the lock or downloads
replacement code. Four tests cover those failure cases and line-ending parity.

Build Hunter from its repository with `pio run -e tdongle_c5_hunter` using the
existing pinned C5 toolchain. Its flash tools retain serial-number selection and
the occupied-port refusal; no hardware has been flashed during this separation.

Configure standalone Core with CMake using explicit
`LAYERTIME_SENSORS_DIR` and, for tests, `LAYERTIME_CONTRACTS_DIR` paths. Enable
`LAYERTIME_BUILD_TESTS` to run the 21 core-only suites. Device model/boundary and
cross-language tests remain integration checks in the original workspace.

Vixen's Android project lives in `android/`. Its existing application IDs and
Kotlin namespaces are preserved for update compatibility; app display strings
including notifications and exports use Vixen. Its native runtime path was adjusted for the new directory depth.
Downloaded inference source, native Wireshark build dependencies, local models,
signing keys, private captures, old APKs and build caches are not source exports.
The existing public Wireshark teaching reference cache was separately restored
and hash-verified for local tests; it remains ignored by Git.

Local Android verification reuses the installed SDK, staged Wireshark native
libraries/headers, and an ignored checkout of the inference runtime at its locked
revision. They are not added to the source export. The Android build and all
364 Android unit tests pass from the new Vixen checkout. Its Python AI workflow
passes a 168-test run with two skips in the existing training environment.

## Evidence and known baseline failure

Final validation on November 13, 2026:

| Check | Result |
|---|---|
| Standalone Core CMake build and tests | 21 suites passed, using sibling Sensors and Contracts |
| Standalone Hunter board/SD/card-transfer host tests | 517 checks passed |
| Standalone Hunter firmware build | Passed; same reported RAM/flash usage as the integration build |
| Original workspace Hunter build | Passed with extracted libraries |
| T-Watch Ultra firmware build | Passed using its compatible watch toolchain |
| S3 Plus firmware build | Passed |
| Legacy C5 beacon firmware build | Passed |
| Vixen Android debug APK | Built from the new repository |
| Vixen Android unit tests | 364 passed |
| Vixen Python workflow | 168-test run passed with 2 skips, using the installed training environment |
| Shared-library content-lock tests | 4 passed |
| Original full host gate | 22 check groups pass; 1 pre-existing historical fixture mismatch remains |

The Ultra and C5 use different Arduino package versions. Their builds were
verified with separate package directories after their shared package setup
contended; no SDK or firmware version pins were upgraded. Build success is not
a new device or field acceptance claim. No hardware was flashed or app installed.

The initial source inventory is `before-extraction.json`; the current dependency
inventory is `after-extraction.json`. Both name the original LayerTime source
commit `7aec804eaea2387b67dbfaa378d26466d493a491` and fingerprint working-tree
inputs. They are source inventories, not proof of firmware behavior.

The historical `test_sensor_equivalence` fixture already disagrees with the
recorded detector library: one example expects a confirmed Find My tracker while
the actual library reports an unknown Find My beacon type. Building that test
against the original committed core reproduces the same failure. Full emitted
detector output before and after extraction has the identical SHA-256:

`903dde150c23e3302d32e0473ff2aafcdb987555eaaacacea3dee5006dd345cf`

The old oracle was not rewritten to manufacture a passing result. The original
full host gate therefore remains red on this existing mismatch even though its
other 22 check groups pass. This is separate from the 21 passing standalone core
suites. Logs remain under ignored `tools/ci/logs/`.
