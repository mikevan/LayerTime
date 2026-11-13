"""Run Hunter's existing host tests with only sibling shared repositories.

Installed as Hunter/test/run_host_tests.py by prepare_repositories.py.
Requires a native C++17 compiler, not an ESP32 cross-compiler.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    core = root.parent / "LayerTime-Core"
    board = root.parent / "LayerTime-C5"
    sensors = root.parent / "LayerTime-Sensors"
    subprocess.run([sys.executable, str(root / "tools/dependencies_check.py")], check=True)
    includes = [core / "test", core / "src", board / "src", sensors / "src", root / "src"]
    sources = [root / "test/test_tdongle_c5_bringup/test_tdongle_c5_bringup.cpp",
               board / "src/BringUpLogic.cpp", root / "src/SdLogLogic.cpp",
               root / "src/CardTransferLogic.cpp", core / "src/core/link/LinkCard.cpp",
               core / "src/core/link/LinkCodec.cpp"]
    with tempfile.TemporaryDirectory(prefix="hunter-host-tests-") as temp:
        binary = Path(temp) / ("tests.exe" if os.name == "nt" else "tests")
        command = [args.cxx, "-std=c++17", "-O0", "-Wall", "-Wextra"]
        command += ["-I" + str(path) for path in includes]
        command += [str(path) for path in sources] + ["-o", str(binary)]
        subprocess.run(command, check=True, cwd=root)
        subprocess.run([str(binary)], check=True, cwd=root)


if __name__ == "__main__":
    main()
