# Why the T-Watch S3 Plus has its own copy of the lt1 platform

This folder is a byte-for-byte copy of `tools/pioarduino-platform-55.03.36-1-lt1`
(pioarduino platform-espressif32 55.03.36-1 with the LayerTime bootstrap
patch; see LAYERTIME-PATCH.md in this folder, copied unchanged). Only the folder
name differs, and that is the whole point.

## The problem it solves

pioarduino installs a `symlink://` platform by writing one link record in the
shared core directory, named after the folder:

    ~/.platformio/platforms/pioarduino-platform-55.pio-link

Measured 2026-10-01 with pioarduino Core 6.2.0 in a clean core directory:

- Two checkouts that each reference `symlink://${PROJECT_DIR}/tools/pioarduino-platform-55.03.36-1-lt1`
  share that one record. Resolving the environment in the second checkout
  (`pio pkg install`, or the IntelliSense index rebuild `pio project init --ide vscode`
  that the IDE runs when a project is opened) rewrites the record to point at
  the second checkout.
- A folder named `pioarduino-platform-s3plus-55.03.36-1-lt1` gets its own
  record, `pioarduino-platform-s3plus-55.pio-link`, and leaves the first one
  untouched.
- With `default_envs` set to the S3 Plus environment, the IntelliSense rebuild
  resolves only that environment; the Ultra environment's platform is not
  touched.

## How the S3 Plus uses it

`lilygo-s3plus/platformio.ini` (the S3 Plus project, opened on its own like
`garmin/`) references this folder and sets `core_dir = ~/.platformio-s3plus`.
So the S3 Plus installs its platform link, packages, Python environment, and
cache in its own core directory, never in the shared `~/.platformio` the
T-Watch Ultra and the T-Dongle-C5 use. The renamed folder is a second layer:
even if the S3 Plus were ever resolved in the shared core directory (for
example through a PLATFORMIO_CORE_DIR environment variable, which overrides
`core_dir`), it would get its own link record and leave the Ultra's alone.

Both folders hold identical platform content, so both targets build with the
same platform version and Arduino core 3.3.6.

## Rules

- Do not edit files here. If the Ultra's platform ever moves to a new suffix
  (`lt2`), make a matching `pioarduino-platform-s3plus-...-lt2` copy.
