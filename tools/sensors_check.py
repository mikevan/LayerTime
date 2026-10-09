# LayerTime: sensor library guard. One implementation, used two ways:
#   * before every device firmware build, as "pre:../../tools/sensors_check.py"
#     in each device's platformio.ini;
#   * by tools/ci/preflight.sh, as "python3 tools/sensors_check.py <repo root>".
#
# The detectors are LayerTime-Sensors, a separate project checked out as the
# git submodule at sensors/. LayerTime records exactly one revision of it.
# A build or test run is only evidence about that revision if the files it
# compiles ARE that revision, so this guard requires all of:
#   1. sensors/ is present (library.json and src/lts/).
#   2. git is available and the LayerTime folder is a git checkout. A failed
#      lookup is a failure, never a pass: source exports (a folder with no
#      git metadata) are not supported.
#   3. LayerTime records sensors/ as a submodule commit.
#   4. sensors/ is itself a git checkout, at exactly that commit.
#   5. No tracked library file is modified, and there is no untracked file
#      anywhere in the library. Files the library's own .gitignore ignores
#      are allowed only outside src/ and library.json, the build inputs.
#
# Results:
#   PASS          all five hold.
#   FAIL          any of 1-4 fails, or 4-5 fail without the override below.
#   EXPERIMENTAL  4 or 5 fails, and LAYERTIME_SENSORS_UNRECORDED=1 is set.
#                 For deliberate work on the library before its revision is
#                 recorded. A firmware build continues with a warning;
#                 preflight.sh and every acceptance runner treat it as NOT
#                 VALID FOR ACCEPTANCE and exit nonzero. The override never
#                 relaxes 1-3.
#
# Command line exit codes: 0 PASS, 1 FAIL, 3 EXPERIMENTAL.
#
# Read-only. git runs with --no-optional-locks, so status never rewrites an
# index or leaves a lock file behind.

import os
import shutil
import subprocess
import sys

OVERRIDE = "LAYERTIME_SENSORS_UNRECORDED"
BUILD_INPUTS = ("src/", "library.json")


def _git(args, cwd, git):
    """(ok, stdout) for a git command; ok is False on any failure."""
    try:
        r = subprocess.run([git, "--no-optional-locks"] + args, cwd=cwd,
                           capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as e:
        return False, str(e)
    return r.returncode == 0, (r.stdout if r.returncode == 0 else r.stderr).strip()


def _same(a, b):
    n = lambda p: os.path.normcase(os.path.realpath(os.path.abspath(p)))
    return n(a) == n(b)


def check(repo_root, environ=None, git="git"):
    """Returns (status, message); status is "PASS", "FAIL", or "EXPERIMENTAL"."""
    environ = os.environ if environ is None else environ
    sensors = os.path.join(repo_root, "sensors")
    fetch = "Fetch it, from the LayerTime folder, with:\n    git submodule update --init sensors"

    if not (os.path.isfile(os.path.join(sensors, "library.json")) and
            os.path.isfile(os.path.join(sensors, "src", "lts", "Detectors.h"))):
        return "FAIL", "LayerTime-Sensors is missing: " + sensors + " is empty or incomplete.\n" + fetch

    if shutil.which(git) is None:
        return "FAIL", ("git was not found, so the library revision cannot be checked. "
                        "Install git or put it on PATH.")
    ok, top = _git(["rev-parse", "--show-toplevel"], repo_root, git)
    if not ok or not _same(top, repo_root):
        return "FAIL", (repo_root + " is not a git checkout (" + top + "). The library revision "
                        "cannot be verified; source exports without git metadata are not supported.")
    ok, rec = _git(["ls-tree", "HEAD", "sensors"], repo_root, git)
    parts = rec.split()
    if not ok or len(parts) < 3 or parts[1] != "commit":
        return "FAIL", "LayerTime does not record sensors/ as a submodule commit (" + (rec or "no entry") + ")."
    want = parts[2]

    ok, stop = _git(["rev-parse", "--show-toplevel"], sensors, git)
    if not ok or not _same(stop, sensors):
        return "FAIL", "sensors/ is not a git checkout of LayerTime-Sensors (" + stop + ").\n" + fetch
    ok, have = _git(["rev-parse", "HEAD"], sensors, git)
    if not ok:
        return "FAIL", "The commit checked out in sensors/ cannot be read (" + have + ")."
    ok, st = _git(["status", "--porcelain=v1", "--untracked-files=all", "--ignored=matching"], sensors, git)
    if not ok:
        return "FAIL", "git status failed in sensors/ (" + st + ")."

    problems = []
    if have != want:
        problems.append("sensors/ is at " + have + " but LayerTime records " + want + ".")
    for line in st.splitlines():
        code, path = line[:2], line[3:].strip('"')
        if code == "!!":
            if path.startswith(BUILD_INPUTS[0]) or path == BUILD_INPUTS[1]:
                problems.append("ignored file inside the build inputs: " + path)
        elif code == "??":
            problems.append("untracked file: " + path)
        else:
            problems.append("modified tracked file (" + code.strip() + "): " + path)

    if not problems:
        return "PASS", "LayerTime-Sensors: " + have + ", the recorded revision, with a clean checkout."
    detail = "\n".join("    " + p for p in problems)
    if environ.get(OVERRIDE) == "1":
        return "EXPERIMENTAL", ("LayerTime-Sensors is NOT the recorded revision:\n" + detail + "\n"
                                + OVERRIDE + "=1 is set, so this is an EXPERIMENTAL build. "
                                "It is NOT VALID FOR ACCEPTANCE.")
    return "FAIL", ("LayerTime-Sensors in sensors/ does not match the revision LayerTime records:\n"
                    + detail + "\nRestore it, from the LayerTime folder, with:\n"
                    "    git submodule update sensors\n"
                    "(and remove or commit any changes inside sensors/ first).")


EXIT = {"PASS": 0, "FAIL": 1, "EXPERIMENTAL": 3}

try:
    Import("env")  # noqa: F821  (SCons: running as a pioarduino pre-script)
except NameError:
    env = None

if env is not None:
    root = os.path.normpath(os.path.join(env.subst("$PROJECT_DIR"), "..", ".."))
    status, message = check(root)
    print("")
    if status == "PASS":
        print("SENSOR LIBRARY CHECK: " + message)
    elif status == "EXPERIMENTAL":
        print("SENSOR LIBRARY CHECK: EXPERIMENTAL BUILD, NOT VALID FOR ACCEPTANCE.\n" + message)
    else:
        print("SENSOR LIBRARY CHECK: build stopped.\n" + message)
    print("")
    if status == "FAIL":
        env.Exit(1)  # noqa: F821
elif __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: sensors_check.py <LayerTime folder>", file=sys.stderr)
        sys.exit(2)
    status, message = check(sys.argv[1])
    print(status + ": " + message)
    sys.exit(EXIT[status])
