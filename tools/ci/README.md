# LayerTime pre-flight gate

One script, `tools/ci/preflight.sh`, run locally by contributors and by CI.
Prove your change locally before you push; CI runs the identical script.

## Profiles
- `--host`  host unit suites + architecture boundary + sensor harness. Needs
  only a HOST C++ compiler. PASS means "host checks passed", never "firmware
  builds".
- `--full`  `--host` plus a build of the required firmware targets
  (`--targets a,b`, default `twatch_ultra`). This is the merge gate. A required
  firmware target that cannot run is reported NOT RUN and FAILS the profile.
- `--docker`  run the selected profile inside the pinned image for an
  environment that matches CI. Not available yet: the image is not built or
  published, so CI runs only the host job for now.

Results are reported in three buckets: PASS, FAIL, NOT RUN. The run's inputs
(source commit, host compiler, device dependency pins, container digest) are
written to `tools/ci/logs/preflight_manifest.txt`.

## Supported environments
- Linux/macOS with `g++`/`clang++` and `bash`: run `./tools/ci/preflight.sh`.
- Windows: run `tools\ci\preflight.ps1`. It runs the host profile under WSL2
  (which must have a host g++, e.g. `build-essential`), or, with `--docker`,
  inside the pinned image via Docker Desktop. An ESP32 cross-compiler is NOT a
  host compiler and cannot run the host suites.
- Device firmware builds (`--full`) use the pioarduino toolchain, native or in
  the pinned image (`.devcontainer/Dockerfile`).

## Parity
Running the same script is necessary but not sufficient for a verdict that
matches CI. Verdicts are comparable only when the recorded inputs match:
compiler version, pioarduino toolchain, the pinned device dependencies, the
platform, and (under `--docker`) the image digest. The manifest records these
so a local PASS can be checked against CI's inputs.
