# LayerTime: sensor library check, run before every device firmware build.
#
# Loaded by each device's platformio.ini as "pre:../../tools/sensors_check.py".
# The detectors are LayerTime-Sensors, a separate project checked out as the
# git submodule at sensors/. LayerTime records exactly one revision of it, and
# every device build and host test must compile that revision. This script
# stops the build, and says how to fix it, when:
#   * sensors/ is missing or empty (the submodule was never fetched), or
#   * the checked-out library is not the revision LayerTime records.
# For deliberate work on a newer library before recording it, set the
# environment variable LAYERTIME_SENSORS_UNRECORDED=1; the build then says
# which revision it is using instead of stopping.
#
# Read-only: it runs "git ls-tree" and "git rev-parse", which never take a
# lock or change a file.

import os
import subprocess

try:
    Import("env")  # noqa: F821  (SCons)
except NameError:  # imported by a host test
    env = None


def _git(args, cwd):
    try:
        out = subprocess.run(["git"] + args, cwd=cwd, capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.SubprocessError):
        return None
    return out.stdout.strip() if out.returncode == 0 else None


def check(repo_root):
    """Returns (ok, message)."""
    sensors = os.path.join(repo_root, "sensors")
    manifest = os.path.join(sensors, "library.json")
    header = os.path.join(sensors, "src", "lts", "Detectors.h")
    if not (os.path.isfile(manifest) and os.path.isfile(header)):
        return False, ("LayerTime-Sensors is missing: " + sensors + " is empty or incomplete.\n"
                       "Fetch it, from the LayerTime folder, with:\n"
                       "    git submodule update --init sensors")
    recorded = _git(["ls-tree", "HEAD", "sensors"], repo_root)
    if recorded is None or not recorded.split()[1:2] == ["commit"]:
        return True, "LayerTime-Sensors: in-tree folder (no recorded submodule revision)."
    want = recorded.split()[2]
    have = _git(["rev-parse", "HEAD"], sensors)
    if have == want:
        return True, "LayerTime-Sensors: " + have + " (the revision LayerTime records)."
    msg = ("LayerTime-Sensors in sensors/ is at " + str(have) + ",\n"
           "but LayerTime records " + want + ".\n"
           "Check out the recorded revision, from the LayerTime folder, with:\n"
           "    git submodule update sensors")
    if os.environ.get("LAYERTIME_SENSORS_UNRECORDED") == "1":
        return True, msg + "\nLAYERTIME_SENSORS_UNRECORDED=1 is set, so this build continues with " + str(have) + "."
    return False, msg


if env is not None:
    root = os.path.normpath(os.path.join(env.subst("$PROJECT_DIR"), "..", ".."))
    ok, message = check(root)
    print("")
    print(("SENSOR LIBRARY CHECK: " if ok else "SENSOR LIBRARY CHECK: build stopped. ") + message)
    print("")
    if not ok:
        env.Exit(1)  # noqa: F821
