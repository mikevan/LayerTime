# GPS / sensor diagnostic

Temporary diagnostic firmware. **It replaces LayerTime on the watch.**
Reflash LayerTime from the repo root when you are done.

It touches nothing in the parent project. It borrows `boards/` and
`variants/lilygo_twatch_ultra/` from the repo root so it cannot drift from
the real build.

## What it answers

1. **What is on the I2C bus.** Specifically whether a magnetometer exists.
   That decides whether dead reckoning takes a live heading or asks the user
   for his azimuth.
2. **What the MIA-M10Q does when it loses the constellation.** It watches for
   the position staying identical while `iTOW` keeps advancing, and says so
   out loud. That is the stale-fix hazard, and no amount of reading
   `GpsService.cpp` answers it.
3. **Whether `hAcc`, the spoofing detector, and the jamming detector report
   anything.** All three are UBX only. The firmware currently reads NMEA,
   which cannot carry any of them.

## Run it (VS Code)

This is a second PlatformIO project, so VS Code needs to be told it exists.

1. **File > Add Folder to Workspace...** and pick `tools\gpsdiag`.
2. PlatformIO sidebar > **gpsdiag** > **env:twatch_ultra_diag** > General > **Upload**.
   The environment is named `twatch_ultra_diag` so it cannot be confused with
   LayerTime's `twatch_ultra` in the same sidebar.
3. Then **Monitor** under the same environment.

The monitor logs itself. `monitor_filters` includes `log2file`, which writes
`platformio-device-monitor-*.log` into this folder, and `time`, which stamps
every line with wall clock. No piping, no Tee-Object, nothing to remember.

**The test:** get a fix near a window, wait for `fix=3D` and a small `hAcc`,
then carry the watch into the middle of the house and leave it running for
ten minutes.

**Put LayerTime back afterward:** PlatformIO sidebar > **LayerTime** >
**env:twatch_ultra** > General > **Upload**.

## Reading the output

- `hAcc` is the receiver's own horizontal accuracy estimate, in millimetres.
  This is the number the uncertainty circle should be driven by. HDOP is a
  satellite geometry factor, not an accuracy, and it is what the firmware
  uses today.
- `POSITION UNCHANGED for N ms while iTOW advances` is the stale-fix
  condition reproduced on the bench.
- `spoofDetState` and `jammingState` should read 1 at a desk. Anything else
  at a desk means either the bit positions in this tool are wrong or
  something interesting is going on.

## Trust nothing here

Every decoded field is printed alongside the raw payload hex on purpose.
The UBX offsets and bit positions in `main.cpp` were written from memory and
are marked where they are least certain. **Check them against the u-blox M10
SPG interface description before any of this becomes firmware.**

Interface description: https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf
