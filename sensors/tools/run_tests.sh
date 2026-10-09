#!/usr/bin/env bash
# Builds and runs every LayerTime-Sensors test with a plain host C++
# compiler. The library is first copied into an empty scratch folder, so the
# tests run with nothing else on disk around them: if anything in the library
# reached outside itself, the build would fail here.
#
#   tools/run_tests.sh            all suites
#   tools/run_tests.sh <suite>    one suite, e.g. test_wifi_classifier
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ -f "$here/library.json" ] && [ -f "$here/src/lts/Detectors.h" ] || {
  echo "run_tests: $here is not a LayerTime-Sensors checkout (library.json or src/lts missing)." >&2; exit 2; }
CXX="${CXX:-g++}"
command -v "$CXX" >/dev/null 2>&1 || { echo "run_tests: no host C++ compiler ($CXX) found." >&2; exit 2; }

scratch="$(mktemp -d)"; trap 'rm -rf "$scratch"' EXIT
iso="$scratch/LayerTime-Sensors"
mkdir -p "$iso"
cp -R "$here/src" "$here/test" "$here/tools" "$here/library.json" "$iso/"

echo "LayerTime-Sensors tests, isolated copy in $iso"
echo "compiler: $("$CXX" --version | head -n1)"
pass=0; fail=0
for dir in "$iso"/test/test_*/; do
  name="$(basename "$dir")"
  [ $# -gt 0 ] && [ "$name" != "$1" ] && continue
  if ! ( cd "$iso/test" && "$CXX" -std=c++17 -O0 -Wall -Wextra -Werror -I. -I../src \
        -o "$scratch/$name" "$name/$name.cpp" ../src/lts/*.cpp ) >"$scratch/$name.cc" 2>&1; then
    echo "FAIL  $name (compile)"; sed 's/^/      /' "$scratch/$name.cc" | tail -n 20; fail=$((fail+1)); continue
  fi
  if ( cd "$iso/test" && "$scratch/$name" ) >"$scratch/$name.run" 2>&1; then
    echo "PASS  $name ($(tail -n1 "$scratch/$name.run"))"; pass=$((pass+1))
  else
    echo "FAIL  $name"; grep -v '^pass ' "$scratch/$name.run" | sed 's/^/      /' | tail -n 30; fail=$((fail+1))
  fi
done
echo "LAYERTIME-SENSORS: $pass passed, $fail failed"
[ "$fail" -eq 0 ] && [ "$pass" -gt 0 ]
