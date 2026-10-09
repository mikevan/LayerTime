#!/usr/bin/env bash
# LayerTime pre-flight gate. Run it locally before every push. LayerTime does
# not use GitHub Actions or any other hosted CI; this script is the gate.
#
# PROFILES (pick one):
#   --host    the sensor library suite (LayerTime-Sensors, isolated), host unit
#             suites, architecture boundary, and sensor harness.
#             Needs only a HOST C++ compiler (g++/clang++). No device toolchain.
#   --full    everything in --host PLUS a build of the required firmware
#             targets, each from its own device project (see PROJECT below).
#   --targets a,b   firmware environments --full builds (default: twatch_ultra).
#             Known: twatch_ultra, twatch_s3plus, tdongle_c5_wand. Any other
#             name stops the run before anything is built.
#
# A required check that cannot run is reported NOT RUN and FAILS the profile.
# --host does not require firmware; it reports the firmware build as NOT RUN
# and its PASS means "host checks passed", never "firmware builds".
#
# The library revision guard (tools/sensors_check.py) runs first. FAIL stops
# the verdict from passing. With LAYERTIME_SENSORS_UNRECORDED=1 a mismatched
# or modified library is reported EXPERIMENTAL; the run then ends "NOT VALID
# FOR ACCEPTANCE" and exits 3, never 0.
#
# Exit codes: 0 passed, 1 failed, 2 usage or setup error, 3 experimental.
# The run's inputs (source commit, library commit, compiler, device
# dependency pins) are written to tools/ci/logs/preflight_manifest.txt.
set -uo pipefail

profile="host"; targets="twatch_ultra"
while [ $# -gt 0 ]; do case "$1" in
  --host) profile="host";; --full) profile="full";;
  --targets) shift; targets="${1:-}";;
  *) echo "unknown argument: $1" >&2; exit 2;; esac; shift; done
IFS=',' read -r -a TARGETS <<< "$targets"

# Firmware environment -> the device project that defines it.
declare -A PROJECT=(
  [twatch_ultra]=devices/lilygo-tultra
  [twatch_s3plus]=devices/lilygo-s3plus
  [tdongle_c5_wand]=devices/lilygo-layerwand
)
[ "${#TARGETS[@]}" -gt 0 ] || { echo "pre-flight: --targets is empty." >&2; exit 2; }
for t in "${TARGETS[@]}"; do
  [ -n "${PROJECT[$t]+x}" ] || { echo "pre-flight: unknown firmware target '$t'. Known: ${!PROJECT[*]}" >&2; exit 2; }
done

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$here"
while [ "$root" != "/" ] && [ ! -f "$root/src/core/logic/ReconClassification.cpp" ]; do root="$(dirname "$root")"; done
[ -f "$root/src/core/logic/ReconClassification.cpp" ] || { echo "pre-flight: repository root not found." >&2; exit 2; }
# The sensor library is a submodule at sensors/. Stop with the fix, not with
# a wall of missing-header errors, when it has not been fetched.
if [ ! -f "$root/sensors/library.json" ] || [ ! -f "$root/sensors/src/lts/Detectors.h" ]; then
  echo "pre-flight: LayerTime-Sensors is missing: sensors/ is empty or not checked out." >&2
  echo "pre-flight: fetch it with: git submodule update --init sensors" >&2
  exit 2
fi
harness=""; [ -f "$root/test/replay/test_sensor_replay.cpp" ] && harness="$root/test/replay"

CXX="${CXX:-g++}"; FLAGS="-std=c++17 -O0 -Wall -Wextra"
out="$(mktemp -d)"; trap 'rm -rf "$out"' EXIT
T="$root/test"; S="$root/src"; L="$S/core/logic"; A="$S/core/app"; K="$S/core/link"
Z="$root/sensors/src"; ZS=("$Z"/lts/*.cpp)
logdir="$root/tools/ci/logs"; mkdir -p "$logdir"
PASS=(); FAIL=(); NOTRUN=()

host_check() { # name, sources...
  local name="$1"; shift
  if ! "$CXX" $FLAGS -I"$T" -I"$S" -I"$Z" ${harness:+-I"$harness"} -o "$out/$name" "$@" 2>"$out/$name.cc"; then
    FAIL+=("$name (compile)"); return; fi
  if ! ( cd "$T" && "$out/$name" ) >"$out/$name.run" 2>&1; then
    FAIL+=("$name (run: $(tail -n1 "$out/$name.run"))"); return; fi
  PASS+=("$name ($(tail -n1 "$out/$name.run"))")
}

# Source identity: the commit under test, read straight from .git without
# running git, so the gate never takes a git lock on a working checkout.
# Uncommitted edits are NOT captured here.
source_identity() {
  local g="$root/.git" head ref
  [ -f "$g/HEAD" ] || { echo "unknown (no .git)"; return; }
  head="$(tr -d '\r\n' < "$g/HEAD")"
  case "$head" in
    ref:*) ref="${head#ref: }"
      if [ -f "$g/$ref" ]; then echo "$(tr -d '\r\n' < "$g/$ref") ($ref; working tree may hold uncommitted edits)"
      else echo "$(grep " $ref\$" "$g/packed-refs" 2>/dev/null | cut -d' ' -f1) ($ref, packed; working tree may hold uncommitted edits)"; fi;;
    *) echo "$head (detached; working tree may hold uncommitted edits)";;
  esac
}

# The sensor library's own commit when sensors/ is a submodule checkout (its
# .git is a file pointing into the superproject's .git/modules), read without
# running git, like source_identity.
sensors_identity() {
  local s="$root/sensors" gd head
  if [ -f "$s/.git" ]; then
    gd="$(sed -n 's/^gitdir: //p' "$s/.git" | tr -d '\r')"
    case "$gd" in /*) ;; *) gd="$s/$gd";; esac
    head="$(tr -d '\r\n' < "$gd/HEAD" 2>/dev/null)"
    case "$head" in ref:*) head="$(tr -d '\r\n' < "$gd/${head#ref: }" 2>/dev/null)";; esac
    echo "${head:-unknown} (submodule checkout; its working tree may hold uncommitted edits)"
  elif [ -d "$s/.git" ]; then
    echo "standalone clone at sensors/ (not the recorded submodule)"
  else
    echo "in-tree folder (not a submodule)"
  fi
}

# ---------------- input manifest (source identity + toolchain) --------------
manifest="$logdir/preflight_manifest.txt"
{
  echo "# LayerTime pre-flight input manifest"
  echo "timestamp_utc = $(date -u +%FT%TZ)"
  echo "profile       = $profile"
  echo "targets       = $targets"
  echo "host_uname    = $(uname -srm)"
  echo "host_cxx      = $("$CXX" --version 2>/dev/null | head -n1)"
  echo "source_identity = $(source_identity)"
  echo "sensors_identity = $(sensors_identity)"
  echo "# device dependency pins (not upgraded by the gate):"
  grep -hE '^\s*(https://|[a-z0-9_-]+/[A-Za-z0-9_-]+ @|platform = )' "$root/devices/lilygo-tultra/platformio.ini" 2>/dev/null | sed 's/^/  dep: /'
  echo "parity_note = two verdicts are comparable only when the inputs above match"
} > "$manifest"

echo "LayerTime pre-flight"
echo "profile: $profile    firmware targets: $targets"
echo "inputs recorded in: tools/ci/logs/preflight_manifest.txt"
echo

# ---------------- Tier 0: host checks (both profiles) -----------------------
echo "Sensor library revision (tools/sensors_check.py):"
# The same guard the firmware builds run: the recorded commit, a clean
# library checkout, readable git metadata. FAIL and EXPERIMENTAL both keep
# this run from passing.
EXPERIMENTAL=0
if ! command -v python3 >/dev/null 2>&1; then
  FAIL+=("sensors revision (python3 not found; the guard cannot run)")
else
  guard="$(python3 "$root/tools/sensors_check.py" "$root" 2>&1)"; gcode=$?
  echo "$guard" | sed 's/^/  /'
  case $gcode in
    0) PASS+=("sensors revision ($(sensors_identity | cut -d' ' -f1), recorded and clean)");;
    3) EXPERIMENTAL=1; NOTRUN+=("sensors revision (EXPERIMENTAL: not the recorded, clean library)");;
    *) FAIL+=("sensors revision (guard failed; see the lines above)");;
  esac
fi
if bash "$root/tools/ci/test_sensors_guard.sh" >"$out/guard_tests.log" 2>&1; then
  PASS+=("sensors guard negative tests ($(tail -n1 "$out/guard_tests.log"))")
else
  FAIL+=("sensors guard negative tests (see tools/ci/logs/guard_tests.log)"); cp "$out/guard_tests.log" "$logdir/guard_tests.log"
fi
echo
echo "Sensor library (LayerTime-Sensors, built and run in an isolated copy):"
if bash "$root/sensors/tools/run_tests.sh" >"$out/sensors.log" 2>&1; then
  PASS+=("sensors library ($(tail -n1 "$out/sensors.log"))")
else
  FAIL+=("sensors library (see tools/ci/logs/sensors.log)"); cp "$out/sensors.log" "$logdir/sensors.log"
fi
echo
echo "Host unit suites + architecture boundary + sensor harness:"
host_check geogrid            "$T/test_geogrid/test_geogrid.cpp" "$L/GeoGrid.cpp"
host_check core_model         "$T/test_core_model/test_core_model.cpp" "$L/QuickMessages.cpp"
host_check decl_advice        "$T/test_declination_advice/test_declination_advice.cpp" "$L/DeclinationAdvice.cpp"
host_check recon_selection    "$T/test_recon_selection/test_recon_selection.cpp" "$L/ReconSelection.cpp"
host_check recon_classification "$T/test_recon_classification/test_recon_classification.cpp" "$L/ReconClassification.cpp" "$L/ReconSelection.cpp" "${ZS[@]}"
host_check alert_policy       "$T/test_alert_policy/test_alert_policy.cpp" "$L/AlertPolicy.cpp"
host_check recon_scheduler    "$T/test_recon_scheduler/test_recon_scheduler.cpp" "$L/ReconScheduler.cpp" "$L/ReconSelection.cpp"
host_check recon_stage_log    "$T/test_recon_stage_log/test_recon_stage_log.cpp" "$L/ReconStageLog.cpp"
host_check mesh_conversations "$T/test_mesh_conversations/test_mesh_conversations.cpp" "$L/MeshConversations.cpp"
host_check detection_csv      "$T/test_detection_csv/test_detection_csv.cpp" "$L/DetectionCsv.cpp"
host_check monitor_event_log  "$T/test_monitor_event_log/test_monitor_event_log.cpp" "$L/MonitorEventLog.cpp" "$L/AlertPolicy.cpp" "$L/ReconSelection.cpp"
host_check layertime_core     "$T/test_layertime_core/test_layertime_core.cpp" "$A/LayerTimeCore.cpp" "$L/MonitorEventLog.cpp" "$L/AlertPolicy.cpp" "$L/MeshConversations.cpp" "$L/QuickMessages.cpp"
host_check mesh_commands      "$T/test_mesh_commands/test_mesh_commands.cpp" "$A/LayerTimeCore.cpp" "$L/MonitorEventLog.cpp" "$L/AlertPolicy.cpp" "$L/MeshConversations.cpp" "$L/QuickMessages.cpp"
host_check settings_commands  "$T/test_settings_commands/test_settings_commands.cpp" "$A/LayerTimeCore.cpp" "$L/MonitorEventLog.cpp" "$L/AlertPolicy.cpp" "$L/MeshConversations.cpp" "$L/QuickMessages.cpp"
host_check link_codec         "$T/test_link_codec/test_link_codec.cpp" "$K/LinkCodec.cpp"
host_check link_vectors_mc    "$T/test_link_vectors_mc/test_link_vectors_mc.cpp"
host_check link_server        "$T/test_link_server/test_link_server.cpp" "$K/LinkServer.cpp" "$K/ChangeTracker.cpp" "$K/LinkCodec.cpp" "$A/LayerTimeCore.cpp" "$L/MonitorEventLog.cpp" "$L/AlertPolicy.cpp" "$L/MeshConversations.cpp" "$L/QuickMessages.cpp"
host_check boundary           "$T/test_boundary/test_boundary.cpp"
host_check sensor_equivalence "$T/test_sensor_equivalence/test_sensor_equivalence.cpp" "$L/ReconClassification.cpp" "$L/ReconSelection.cpp" "${ZS[@]}"
if [ -n "$harness" ]; then host_check sensor_replay "$harness/test_sensor_replay.cpp" "$L/ReconClassification.cpp" "$L/ReconSelection.cpp" "$L/MonitorEventLog.cpp" "$L/AlertPolicy.cpp" "${ZS[@]}"
else FAIL+=("sensor_replay (harness not found)"); fi

# ---------------- Tier 1: firmware build (full profile) ---------------------
firmware_required=0; [ "$profile" = "full" ] && firmware_required=1
echo
echo "Firmware build (required by --full):"
if [ "$profile" != "full" ]; then
  for t in "${TARGETS[@]}"; do NOTRUN+=("firmware:$t (not required by --host)"); done
  echo "  NOT RUN - --host does not build firmware. Use --full for the merge gate."
else
  if command -v pio >/dev/null 2>&1; then
    for t in "${TARGETS[@]}"; do
      # Each environment builds from the device project that defines it.
      dev="$root/${PROJECT[$t]}"
      if ( cd "$dev" && pio run -e "$t" ) >"$out/fw_$t.log" 2>&1; then PASS+=("firmware:$t (${PROJECT[$t]})")
      else FAIL+=("firmware:$t (${PROJECT[$t]}; see tools/ci/logs/fw_$t.log)"); fi
      cp "$out/fw_$t.log" "$logdir/fw_$t.log" 2>/dev/null
    done
  else
    for t in "${TARGETS[@]}"; do NOTRUN+=("firmware:$t (pioarduino core 'pio' not on PATH)"); done
  fi
fi

# ---------------- verdict ---------------------------------------------------
echo; echo "===== RESULTS ($profile profile) ====="
echo "PASS (${#PASS[@]}):";   for x in "${PASS[@]:-}";   do [ -n "$x" ] && echo "  + $x"; done
echo "FAIL (${#FAIL[@]}):";   for x in "${FAIL[@]:-}";   do [ -n "$x" ] && echo "  - $x"; done
echo "NOT RUN (${#NOTRUN[@]}):"; for x in "${NOTRUN[@]:-}"; do [ -n "$x" ] && echo "  ? $x"; done
echo

required_notrun=0
if [ "$firmware_required" -eq 1 ]; then for x in "${NOTRUN[@]:-}"; do case "$x" in firmware:*) required_notrun=1;; esac; done; fi

if [ "${#FAIL[@]}" -eq 0 ] && [ "$required_notrun" -eq 0 ] && [ "$EXPERIMENTAL" -eq 1 ]; then
  echo "EXPERIMENTAL: the checks ran against a library that is not the recorded, clean revision."
  echo "NOT VALID FOR ACCEPTANCE (LAYERTIME_SENSORS_UNRECORDED=1)."
  exit 3
elif [ "${#FAIL[@]}" -eq 0 ] && [ "$required_notrun" -eq 0 ]; then
  if [ "$profile" = "host" ]; then echo "HOST PROFILE PASSED: host checks passed. Firmware build NOT RUN (this does not certify a firmware build)."
  else echo "FULL PROFILE PASSED: host checks and required firmware targets built."; fi
  exit 0
else
  [ "$required_notrun" -eq 1 ] && echo "PROFILE FAILED: a required firmware target could not run (NOT RUN counts as failure for --full)."
  [ "${#FAIL[@]}" -ne 0 ] && echo "PROFILE FAILED: ${#FAIL[@]} check(s) failed."
  exit 1
fi
