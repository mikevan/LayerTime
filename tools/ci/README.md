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
  | `tdongle_c5_wand` | `devices/lilygo-layerwand` |

  Any other name stops the run before anything is built. A required firmware
  target that cannot run (no `pio` on PATH) is reported NOT RUN and fails the
  profile.

Results are reported as PASS, FAIL, and NOT RUN. Exit codes: 0 passed,
1 failed, 2 usage or setup error, 3 experimental (below). The run's inputs
(source commit, library commit, host compiler, device dependency pins) are
written to `tools/ci/logs/preflight_manifest.txt`.

## The sensor library guard
`tools/sensors_check.py` is the single check, used by this script and by
every device firmware build. It requires the recorded `sensors/` submodule
commit, a clean library checkout (no modified tracked files, no untracked
files, no ignored files inside `src/` or `library.json`), and readable git
metadata; a source export without git metadata is not supported.
`LAYERTIME_SENSORS_UNRECORDED=1` allows experimental work on an unrecorded
or modified library: firmware builds continue with a warning, and this
script reports EXPERIMENTAL, prints NOT VALID FOR ACCEPTANCE, and exits 3.
`tools/ci/test_sensors_guard.sh` proves each of those cases.

## Supported environments
- Linux or macOS with `g++`/`clang++`, `bash`, `python3`, and `git`: run
  `./tools/ci/preflight.sh`.
- Windows: run `tools\ci\preflight.ps1`, which runs the profile under WSL2
  (WSL needs `build-essential`, `python3`, and `git`). An ESP32
  cross-compiler is not a host compiler and cannot run the host suites.
- Firmware builds (`--full`) need the pioarduino core's `pio` on PATH.
