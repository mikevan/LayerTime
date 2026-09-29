# LayerTime Connect IQ Device App

The Garmin side of LayerTime Slice 1: a foreground Connect IQ Device App for
the tactix 8 AMOLED (Connect IQ device id `fenix847mm`, which also covers the
fēnix 8 and quatix 8 47/51 mm). Increment 0 is the skeleton: it builds,
installs, launches, draws, and counts physical button presses. LayerTime Link
arrives in Increment 1.

Requires Connect IQ SDK 9.2.0 or later and the Monkey C extension for
VS Code. Open this `garmin/` folder as the workspace root, or add it as a
folder, so the extension sees `manifest.xml`.

## Build and run in the simulator

1. Open the command palette and choose **Monkey C: Build Current Project**,
   then pick **fenix847mm** as the product.
2. Choose **Run > Start Debugging**. The simulator opens with LayerTime.

## Install on the watch

Choose **Monkey C: Build for Device**, pick **fenix847mm**, and copy the
resulting `.prg` from `bin/` into the watch's `GARMIN/Apps/` folder over USB.

## Unit tests

The tests in `test/` use the SDK's Run No Evil framework. In VS Code, open
the Test Explorer (the flask icon) and run them, or from the command line
build with `--unit-test` and run `monkeydo <prg> fenix847mm /t` with the
simulator already open.
