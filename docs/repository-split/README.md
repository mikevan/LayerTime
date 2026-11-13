# Canonical repositories and the watch checkout

LayerTime owns the Ultra, S3 Plus and Garmin watch targets and their integration
tests. Shared source is fetched from its owning Git repository at a recorded
commit. Hunter and Vixen are maintained independently.

## Dependencies used by the watches

| Location in LayerTime | Canonical repository | Pinned commit |
|---|---|---|
| `libraries/LayerTime-Core/` | [LayerTime-Core](https://github.com/mikevan/LayerTime-Core) | `9d87bc399b267ef889c43e4c3dbb215443b682df` |
| `contracts/` | [LayerTime-Contracts](https://github.com/mikevan/LayerTime-Contracts) | `2cefe63077ae598148a42b1cb2ae011a9fa54841` |
| `sensors/` | [LayerTime-Sensors](https://github.com/mikevan/LayerTime-Sensors) | `35e7213874d4577bd9385f226b20e614a65fbfa8` |

These locations are Git submodules. The parent repository stores gitlinks,
while the shared repositories own their source files and history. The pins also
appear in `shared-repositories.lock.json`. The sensor revision preserves the
previously tested detector implementation.

Initialize a fresh checkout with:

```powershell
git clone --recurse-submodules https://github.com/mikevan/LayerTime.git
```

After changing an existing checkout to this revision, run:

```powershell
git submodule update --init --recursive
python tools/shared_repos_check.py
```

The same guard runs before both watch firmware builds. It checks agreement
between the lock, `.gitmodules`, index gitlinks and checked-out commits, along
with clean dependency source and absence of ignored extra build inputs. Seven
isolated Git-fixture tests cover its acceptance and failure cases. The index
check permits reviewing a staged dependency update before committing it; the
same gitlinks are recorded in HEAD when that update is committed.

Develop shared code in its owning repository, then update the gitlink and lock
together and rerun the consuming tests. A regular submodule update follows the
recorded commit and does not float to a branch tip.

## Independent products

| Repository | Owned source |
|---|---|
| [Hunter](https://github.com/mikevan/Hunter) | LilyGO T-Dongle-C5 sensing, radio acquisition, BLE transport, SD logs, PCAP evidence, card transfer and diagnostics |
| [Vixen](https://github.com/mikevan/Vixen) | Android analysis application, product-specific AI workflow, teaching assets and evaluation material |
| [LayerTime-C5](https://github.com/mikevan/LayerTime-C5) | C5 pins, display, LED, button/identity logic and board boot precautions |

The parent-owned copies of these products, the duplicated AI workflow and its
collected documentation, and the old one-time extraction/template scripts were
removed. The source was already committed and verified in the owning public
repositories. Root-level watch firmware downloads and historical distribution
artifacts remain in place. Original Git history preserves earlier source and
releases.

Hunter keeps its existing logo. Vixen is the flagship formerly called
LayerScope. The old `tdongle_c5_vixen` environment in Hunter is the legacy
fox-hunt beacon compatibility name.

Untracked local captures, models, signing material, logs and caches were
preserved. The retired local workspace paths are ignored to prevent accidental
reintroduction of their data into the watch repository.

## Build and test evidence

Both watch projects retain their board, SDK and library version pins. The
verification uses the installed compatible watch package directory
`~/.platformio-s3plus`; the Ultra invocation selects it explicitly, and the
S3 Plus configuration already selects it.

With the pioarduino CLI available:

```powershell
$env:PLATFORMIO_CORE_DIR = "$env:USERPROFILE\.platformio-s3plus"
pio run -d devices/lilygo-tultra -e twatch_ultra
pio run -d devices/lilygo-s3plus -e twatch_s3plus
```

The output images remain at:

- `devices/lilygo-tultra/bin/build/twatch_ultra/firmware.bin`
- `devices/lilygo-s3plus/bin/build/twatch_s3plus/firmware.bin`

`tools/ci/preflight.ps1 --host` runs the host, model, architecture, protocol and
replay checks under WSL. Its Git checks use this Windows checkout's line-ending
and file-mode settings only for that invocation. Shared guard and legacy sensor
guard negative tests are part of the host profile. Hunter and Vixen run their
own tests in their owning repositories.

The historical `test_sensor_equivalence` fixture mismatch remains: an older
oracle expects a confirmed Find My tracker for one input, while the pinned
library reports an unknown Find My beacon type. The same failure was reproduced
before extraction. Complete detector output before and after extraction had
identical SHA-256:

`903dde150c23e3302d32e0473ff2aafcdb987555eaaacacea3dee5006dd345cf`

That oracle has not been rewritten or silently excluded. The host gate reports
this existing failure explicitly. Watch build results and checkout verification
for the canonical-repository cleanup are recorded in `cleanup-validation.json`.
No hardware is flashed by these checks.

## Migration history

`before-extraction.json` and `after-extraction.json` are historical inventories
from the initial split. `validation.json` records its original validation;
`publication.json` records the published owning repository commits and public
visibility. They describe that earlier stage, including temporary migration
copies which this cleanup replaced with submodules or external product links.
The read-only inventory tool now reads tracked shared-submodule contents and the
explicit lock instead of scanning old product copies.
