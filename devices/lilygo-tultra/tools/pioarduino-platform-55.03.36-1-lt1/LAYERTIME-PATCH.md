# pioarduino platform-espressif32 55.03.36-1, LayerTime bootstrap patch (lt1)

This folder is a copy of the pioarduino `platform-espressif32` release
55.03.36-1, the platform the T-Watch Ultra firmware is verified on
(Arduino core 3.3.6, ESP-IDF 5.5.2 libs, toolchains 14.2.0 build 20251107,
esptool 5.1.0). `platformio.ini` points the T-Ultra environments at it with

    platform = symlink://${PROJECT_DIR}/tools/pioarduino-platform-55.03.36-1-lt1

Upstream source: https://github.com/pioarduino/platform-espressif32/releases/download/55.03.36-1/platform-espressif32.zip
(release tag 55.03.36-1). License: Apache-2.0, see LICENSE in this folder.

## Why it exists

Upstream 55.03.36-1 carries its own Python bootstrap, `builder/penv_setup.py`,
which runs before every build against the shared `~/.platformio/penv`. Its
dependency table pins the core by URL:

    "platformio": "https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.18.zip",

and reinstalls that archive whenever no installed package is *named*
`platformio` at version 6.1.18. The LayerTime Node (T-Dongle-C5) needs
platform 55.03.311, which requires pioarduino Core 6.1.19 or later, and the
core is installed under the package name `pioarduino`. So every T-Ultra build
on upstream 55.03.36-1 force-installed `pioarduino-core 6.1.18` next to
`pioarduino 6.2.0`, downgrading the shared core and breaking the C5 build
(observed 2026-09-28 and 2026-09-29; reproduced in a clean venv).

Moving the T-Ultra to 55.03.39 (the first release whose bootstrap accepts the
shared core) put the watch on Arduino core 3.3.9, which fails LayerTime's
hardware gate: with Early Warning enabled the watch reset every ~15 s at the
first BLE initialisation, and stopped resetting with Early Warning off
(2026-09-29). That matches the open ESP32-S3 BLE-init crash reported from
core 3.3.7 (espressif/arduino-esp32 #12357). So the T-Ultra stays on 3.3.6.

## Exact change from upstream

One line in `builder/penv_setup.py`, in `python_deps`:

    -    "platformio": "https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.18.zip",
    +    "pioarduino": ">=6.1.19",

This is the same line pioarduino itself adopted in release 55.03.39. With it,
the bootstrap is satisfied by any pioarduino Core 6.1.19 or newer and installs
nothing; if the core were ever older it would install `pioarduino>=6.1.19`
from PyPI rather than an archive. The `elif name == "platformio"` branch and
`PLATFORMIO_URL_VERSION_RE` in the same file are now unused and were left
untouched to keep the diff minimal.

Everything else (`platform.json`, `platform.py`, `builder/`, `boards/`,
`monitor/`) is byte-identical to upstream. Omitted from the copy because the
build does not read them: `examples/` (sample sketches), `misc/svd/`
(peripheral register maps for the debugger's peripheral view), `docs`,
`CONTRIBUTING.md`, `code_of_conduct.md`.

## Maintenance

- Do not edit files here except through a new suffix (`lt2`, ...) and an
  updated `platformio.ini`, so a build is always reproducible from the tree.
- If a future pioarduino release ships an Arduino core that passes the
  T-Ultra BLE gate, retire this folder and pin that release URL instead.
- To verify the patch against upstream: download the 55.03.36-1 zip above and
  `diff -r` it against this folder; the only differing file is
  `builder/penv_setup.py`, by the one line shown.
