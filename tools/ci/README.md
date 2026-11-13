# LayerTime pre-flight gate

One script, `tools/ci/preflight.sh`, run locally before every push. LayerTime
does not use GitHub Actions or any other hosted CI; this script is the gate.

## Profiles
- `--host`  the sensor library revision guard, the sensor library's own
  suites (in an isolated copy), the host unit suites, the architecture
  boundary, the detection-equivalence check, and the replay harness. Needs
  only a host C++ compiler, python3, and git. PASS means "host checks
  passed", never "firmware builds".
- `--full`  `--host` plus a build of each firmware environment named with
  `--targets a,b` (default `twatch_ultra`), each from its own device project:

  | Environment | Device project |
  |---|---|
  | `twatch_ultra` | `devices/lilygo-tultra` |
  | `twatch_s3plus` | `devices/lilygo-s3plus` |

  Any other name stops the run before anything is built. A required firmware
  target that cannot run (no `pio` on PATH) is reported NOT RUN and fails the
  profile.

Results are reported as PASS, FAIL, and NOT RUN. Exit codes: 0 passed,
1 failed, 2 usage or setup error. The run's inputs
(source commit, library commit, host compiler, device dependency pins) are
written to `tools/ci/logs/preflight_manifest.txt`.

## Shared repository pins

Initialize dependencies with `git submodule update --init --recursive` before
running the gate. `shared-repositories.lock.json`, `.gitmodules` and the Git
submodule entries pin Core, Contracts and Sensors. `tools/shared_repos_check.py`
verifies those pins, the checked-out revisions, clean dependency worktrees and
absence of ignored extra build inputs. Both watch firmware builds run the same
check. Its seven isolated Git-fixture tests cover missing, modified, extra,
wrong-revision and wrong-URL dependencies.

`tools/sensors_check.py` remains as the sensor-specific diagnostic, with its
existing 19 negative checks. Its legacy experimental override does not bypass
the stricter shared-repository gate used by the watch builds.

Hunter and Vixen build and test in their own repositories. The LayerTime gate
accepts the two watch firmware targets listed above.

The detector regression uses fixed historical input keys and a reviewed Sensors
1.1.2 oracle. `test/test_sensor_equivalence/review_policy_update.py` derives that
oracle from the preserved historical file using documented policy changes; the
classifier output is checked separately. Both the derivation and detector
comparison pass. See `test/test_sensor_equivalence/REVIEW.md`.

## Supported environments
- Linux or macOS with `g++`/`clang++`, `bash`, `python3`, and `git`: run
  `./tools/ci/preflight.sh`.
- Windows: run `tools\ci\preflight.ps1`, which runs the profile under WSL2
  (WSL needs `build-essential`, `python3`, and `git`). An ESP32
  cross-compiler is not a host compiler and cannot run the host suites.
- Firmware builds (`--full`) need the pioarduino core's `pio` on PATH.
