#!/usr/bin/env bash
# Negative tests for the sensor library guard (tools/sensors_check.py) and for
# the pre-flight gate's handling of it. Builds throwaway git repositories in a
# scratch folder; never touches the checkout it is run from.
#
#   tools/ci/test_sensors_guard.sh                 guard cases (seconds)
#   tools/ci/test_sensors_guard.sh --preflight     also runs preflight.sh on a
#                                                  scratch copy of this checkout
#                                                  (minutes)
#   tools/ci/test_sensors_guard.sh --preflight-case N   only pre-flight case N
#                                                  (1-7), for shells with a time
#                                                  limit per command
#
# Exit 0 only if every case gives its expected result.
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
guard="$here/tools/sensors_check.py"
with_preflight=0; only=""
[ "${1:-}" = "--preflight" ] && with_preflight=1
[ "${1:-}" = "--preflight-case" ] && { with_preflight=1; only="${2:?case number}"; }
scratch="$(mktemp -d)"; trap 'rm -rf "$scratch"' EXIT
pass=0; fail=0
expect() { # name, expected status word, actual exit code, output
  local name="$1" want="$2" code="$3" out="$4" got
  case "$code" in 0) got=PASS;; 1) got=FAIL;; 3) got=EXPERIMENTAL;; *) got="exit$code";; esac
  if [ "$got" = "$want" ]; then echo "PASS  $name (guard said $got)"; pass=$((pass+1))
  else echo "FAIL  $name: expected $want, got $got"; echo "$out" | sed 's/^/      /'; fail=$((fail+1)); fi
}
g() { git -c user.name=t -c user.email=t@t -c protocol.file.allow=always "$@"; }

# ---- fixture: a library repo and a superproject that records it ----
lib="$scratch/lib"; mkdir -p "$lib/src/lts"
( cd "$lib" && g init -q -b main && echo '{"name":"LayerTime-Sensors"}' > library.json &&
  echo '#pragma once' > src/lts/Detectors.h && echo 'int f(){return 1;}' > src/lts/A.cpp &&
  printf 'build/\n*.o\n' > .gitignore && g add -A && g commit -q -m lib )
make_super() { # dest
  local s="$1"; mkdir -p "$s/tools"
  ( cd "$s" && g init -q -b main && cp "$guard" tools/sensors_check.py &&
    g submodule -q add "$lib" sensors && g add -A && g commit -q -m super ) >/dev/null 2>&1
}
run() { local out code; out="$(cd "$1" && env -u LAYERTIME_SENSORS_UNRECORDED ${2:-} python3 tools/sensors_check.py . 2>&1)"; code=$?; echo "$code"; echo "$out" >"$scratch/last.out"; }

s="$scratch/s0"; make_super "$s"
c=$(run "$s"); expect "clean recorded checkout" PASS "$c" "$(cat "$scratch/last.out")"

s="$scratch/s1"; make_super "$s"; echo 'int f(){return 2;}' > "$s/sensors/src/lts/A.cpp"
c=$(run "$s"); expect "modified tracked library source, HEAD still matches" FAIL "$c" "$(cat "$scratch/last.out")"
c=$(run "$s" LAYERTIME_SENSORS_UNRECORDED=1); expect "same, with the override" EXPERIMENTAL "$c" "$(cat "$scratch/last.out")"

s="$scratch/s2"; make_super "$s"; echo 'int g(){return 3;}' > "$s/sensors/src/lts/Extra.cpp"
c=$(run "$s"); expect "untracked library source file" FAIL "$c" "$(cat "$scratch/last.out")"

s="$scratch/s3"; make_super "$s"; echo 'x' > "$s/sensors/src/lts/stray.o"
c=$(run "$s"); expect "ignored file inside src/ (a build input)" FAIL "$c" "$(cat "$scratch/last.out")"

s="$scratch/s4"; make_super "$s"; mkdir -p "$s/sensors/build" && echo x > "$s/sensors/build/out.o"
c=$(run "$s"); expect "ignored build output outside src/ is allowed" PASS "$c" "$(cat "$scratch/last.out")"

s="$scratch/s5"; make_super "$s"; ( cd "$s/sensors" && g commit -q --allow-empty -m newer )
c=$(run "$s"); expect "library HEAD is not the recorded commit" FAIL "$c" "$(cat "$scratch/last.out")"
c=$(run "$s" LAYERTIME_SENSORS_UNRECORDED=1); expect "same, with the override" EXPERIMENTAL "$c" "$(cat "$scratch/last.out")"

s="$scratch/s6"; make_super "$s"; mv "$s/.git" "$s/.git-away"
c=$(run "$s"); expect "superproject git metadata unavailable (source export)" FAIL "$c" "$(cat "$scratch/last.out")"
c=$(run "$s" LAYERTIME_SENSORS_UNRECORDED=1); expect "same, the override does not relax it" FAIL "$c" "$(cat "$scratch/last.out")"

s="$scratch/s7"; make_super "$s"; mkdir -p "$scratch/nogit"; ln -s "$(command -v python3)" "$scratch/nogit/python3"
c=$(cd "$s" && PATH="$scratch/nogit" python3 tools/sensors_check.py . >"$scratch/last.out" 2>&1; echo $?)
expect "git not installed" FAIL "$c" "$(cat "$scratch/last.out")"

s="$scratch/s8"; make_super "$s"; rm -f "$s/sensors/.git"
c=$(run "$s"); expect "library files present but sensors/ is not a git checkout" FAIL "$c" "$(cat "$scratch/last.out")"

s="$scratch/s9"; make_super "$s"; ( cd "$s" && g rm -q --cached sensors && g commit -q -m "drop gitlink" )
c=$(run "$s"); expect "LayerTime does not record a sensors commit" FAIL "$c" "$(cat "$scratch/last.out")"

s="$scratch/s10"; mkdir -p "$s/tools" "$s/sensors"; cp "$guard" "$s/tools/"
c=$(run "$s"); expect "sensors/ empty (submodule never fetched)" FAIL "$c" "$(cat "$scratch/last.out")"

# ---- the firmware pre-script path: a stand-in for SCons ----
scons_case() { # name, superproject, expected outcome (stopped|continued), env
  local name="$1" s="$2" want="$3" out
  mkdir -p "$s/devices/x"
  out="$(cd "$s" && env -u LAYERTIME_SENSORS_UNRECORDED ${4:-} python3 - "$s" 2>&1 <<'PY'
import sys
root = sys.argv[1]
class Env:
    def subst(self, v): return root + "/devices/x"
    def Exit(self, code): raise SystemExit(code)
g = {"__name__": "sensors_check_prescript"}
def Import(name): g["env"] = Env()
g["Import"] = Import
try:
    exec(open(root + "/tools/sensors_check.py").read(), g)
    print("RESULT continued")
except SystemExit as e:
    print("RESULT stopped", e.code)
PY
)"
  if grep -q "RESULT $want" <<<"$out"; then echo "PASS  firmware pre-script: $name ($want)"; pass=$((pass+1))
  else echo "FAIL  firmware pre-script: $name: expected $want"; echo "$out" | sed 's/^/      /'; fail=$((fail+1)); fi
}
scons_case "clean" "$scratch/s0" continued
scons_case "modified library source" "$scratch/s1" stopped
scons_case "modified library source with override (EXPERIMENTAL banner)" "$scratch/s1" continued LAYERTIME_SENSORS_UNRECORDED=1
if ! (cd "$scratch/s1" && LAYERTIME_SENSORS_UNRECORDED=1 python3 - "$scratch/s1" <<'PY' 2>&1 | grep -q "NOT VALID FOR ACCEPTANCE"
import sys
root=sys.argv[1]
class Env:
    def subst(self, v): return root + "/devices/x"
    def Exit(self, code): raise SystemExit(code)
g={"__name__":"x"}
g["Import"]=lambda n: g.__setitem__("env", Env())
exec(open(root+"/tools/sensors_check.py").read(), g)
PY
); then echo "FAIL  firmware pre-script: override banner missing"; fail=$((fail+1)); else echo "PASS  firmware pre-script: override build is labeled NOT VALID FOR ACCEPTANCE"; pass=$((pass+1)); fi
scons_case "git metadata unavailable" "$scratch/s6" stopped

# ---- the pre-flight gate itself, on a scratch copy of this checkout ----
if [ "$with_preflight" -eq 1 ]; then
  cp_tree() { mkdir -p "$1"; cp -a "$here/." "$1/"; rm -rf "$1/tools/ci/logs"; }
  pf() { (cd "$1" && env -u LAYERTIME_SENSORS_UNRECORDED ${2:-} bash tools/ci/preflight.sh ${3:---host} >"$scratch/pf.out" 2>&1; echo $?); }
  pexpect() { # name, expected exit, actual
    if [ "$3" = "$2" ]; then echo "PASS  preflight: $1 (exit $3)"; pass=$((pass+1))
    else echo "FAIL  preflight: $1: expected exit $2, got $3"; tail -n 15 "$scratch/pf.out" | sed 's/^/      /'; fail=$((fail+1)); fi
  }
  want_case() { [ -z "$only" ] || [ "$only" = "$1" ]; }
  p="$scratch/pf"; cp_tree "$p"
  if want_case 1; then pexpect "clean checkout passes" 0 "$(pf "$p")"; fi
  if want_case 2; then
    echo "// local edit" >> "$p/sensors/src/lts/Signatures.cpp"
    pexpect "modified library source fails" 1 "$(pf "$p")"
    ( cd "$p/sensors" && git checkout -q -- src/lts/Signatures.cpp ); fi
  if want_case 3; then
    echo "// local edit" >> "$p/sensors/src/lts/Signatures.cpp"
    pexpect "same with override: EXPERIMENTAL, NOT VALID FOR ACCEPTANCE" 3 "$(pf "$p" LAYERTIME_SENSORS_UNRECORDED=1)"
    if grep -q "NOT VALID FOR ACCEPTANCE" "$scratch/pf.out"; then echo "PASS  preflight: override run prints NOT VALID FOR ACCEPTANCE"; pass=$((pass+1))
    else echo "FAIL  preflight: banner missing"; fail=$((fail+1)); fi
    ( cd "$p/sensors" && git checkout -q -- src/lts/Signatures.cpp ); fi
  if want_case 4; then
    echo 'int stray;' > "$p/sensors/src/lts/Stray.cpp"
    pexpect "untracked library source fails" 1 "$(pf "$p")"
    rm -f "$p/sensors/src/lts/Stray.cpp"; fi
  if want_case 5; then
    mv "$p/.git" "$p/.git-away"
    pexpect "unreadable git metadata fails" 1 "$(pf "$p")"
    mv "$p/.git-away" "$p/.git"; fi
  if want_case 6; then pexpect "unknown --full target is rejected before building" 2 "$(pf "$p" "" "--full --targets twatch_ultra,unknown_env")"; fi
  if want_case 7; then
    # --full maps each environment to its own project: a stand-in pio records where it ran.
    mkdir -p "$scratch/fakebin"; printf '#!/bin/sh\necho "$PWD|$*" >> "%s"\n' "$scratch/pio_calls" > "$scratch/fakebin/pio"; chmod +x "$scratch/fakebin/pio"
    (cd "$p" && PATH="$scratch/fakebin:$PATH" bash tools/ci/preflight.sh --full --targets twatch_ultra,twatch_s3plus,tdongle_c5_wand >"$scratch/pf.out" 2>&1)
    want="$p/devices/lilygo-tultra|run -e twatch_ultra
$p/devices/lilygo-s3plus|run -e twatch_s3plus
$p/devices/lilygo-layerwand|run -e tdongle_c5_wand"
    if [ "$(cat "$scratch/pio_calls" 2>/dev/null)" = "$want" ]; then echo "PASS  preflight: --full builds each environment in its own project"; pass=$((pass+1))
    else echo "FAIL  preflight: --full mapping"; cat "$scratch/pio_calls" 2>/dev/null | sed 's/^/      /'; fail=$((fail+1)); fi
  fi
fi

echo "SENSOR GUARD TESTS: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
