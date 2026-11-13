# LayerTime pre-flight gate

The same `tools/ci/preflight.sh` host gate runs locally and in GitHub Actions.
`.github/workflows/ci.yml` also builds both watch targets on fresh Ubuntu
runners. Hunter and Vixen have their own workflows in their owning repositories.

## Hosted checks

Every push and pull request runs the checks; manual runs are also available.
Jobs use read-only repository access, pinned action commits and recursive
submodules. No local build folders, signing keys or private captures are needed.
Results, firmware binaries and ELF debugging files are retained for 14 days
under the run's Artifacts section. Firmware builds never flash hardware.

The reusable workflow accepts `candidate_repository` and a full
`candidate_revision` SHA for shared-library integration checks. The four shared
repositories call their affected consumers on pushes and pull requests:

| Library | Consumers |
|---|---|
| Core | Both watches, Hunter |
| C5 | Hunter |
| Sensors | Both watches, Hunter |
| Contracts | Both watches, Vixen |

`test_dependency_candidate.py` stages the candidate gitlink and updates the
corresponding lock in an ephemeral runner checkout. A local integration commit
makes the legacy HEAD-based Sensors check and index-based checks agree. Hunter
also computes candidate source hashes. All normal guards and tests still run.
`candidate-validation.json` records the original consumer and exact candidate.
These runs do not push pins, publish releases or change product repositories.
Adopting a tested library remains a product change with its own checked gitlink
and lock update. Candidate artifacts are labelled with the library name; use
the product's `pinned` artifacts for ordinary hardware acceptance.

See [hardware acceptance](../../docs/firmware/hardware-acceptance.md) before
calling a build device-verified. Required merge checks are configured only
after their jobs have successfully run on GitHub.

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
