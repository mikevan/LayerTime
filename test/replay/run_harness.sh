#!/usr/bin/env bash
# One command to build and run the LayerTime sensor replay harness on the host.
# No IDE, no framework, no device: one g++ invocation against the production
# detection sources, then run. Exit code is nonzero on any failed check, so CI
# can gate on it.
#
# It finds the repository root by walking up from this script until it sees
# src/core/logic/ReconClassification.cpp, so it runs from any working directory.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$here"
while [ "$root" != "/" ] && [ ! -f "$root/src/core/logic/ReconClassification.cpp" ]; do
  root="$(dirname "$root")"
done
if [ ! -f "$root/src/core/logic/ReconClassification.cpp" ]; then
  echo "run_harness: could not find the repository root (src/core/logic/ReconClassification.cpp)." >&2
  exit 2
fi

if [ ! -f "$root/sensors/src/lts/Detectors.h" ]; then
  echo "run_harness: LayerTime-Sensors is missing (sensors/src/lts). Fetch it with: git submodule update --init sensors" >&2
  exit 2
fi

out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

g++ -std=c++17 -O0 -Wall -Wextra \
  -I"$root/src" -I"$root/sensors/src" -I"$root/test" -I"$here" \
  -o "$out/test_sensor_replay" \
  "$here/test_sensor_replay.cpp" \
  "$root/src/core/logic/ReconClassification.cpp" \
  "$root/sensors/src/lts/"*.cpp \
  "$root/src/core/logic/ReconSelection.cpp" \
  "$root/src/core/logic/MonitorEventLog.cpp" \
  "$root/src/core/logic/AlertPolicy.cpp"

"$out/test_sensor_replay" "$@"
