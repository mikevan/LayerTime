#!/usr/bin/env bash
# LayerTime pre-flight gate. One script, run locally by contributors and by CI.
#
# PROFILES (pick one):
#   --host    the sensor library suite (LayerTime-Sensors, isolated), host unit
#             suites, architecture boundary, and sensor harness.
#             Needs only a HOST C++ compiler (g++/clang++). No device toolchain.
#   --full    everything in --host PLUS a build of the required firmware
#             targets. This is the merge gate.
#   --docker  run the selected profile inside the pinned image, for an
#             environment that matches CI. Add it to --host or --full.
#   --targets a,b   firmware targets the full profile requires
#                   (default: twatch_ultra).
#
# A required check that cannot run is reported NOT RUN and FAILS the profile.
# --host does not require firmware; it reports the firmware build as NOT RUN
# and its PASS means "host checks passed", never "firmware builds".
#
# Parity note: calling this same script is necessary but NOT sufficient for a
# verdict that matches CI. The verdict is only comparable when the recorded
# inputs match: compiler version, toolchain, dependency versions, platform,
# and (under --docker) the container digest. Those inputs are written to
# tools/ci/logs/preflight_manifest.txt on every run.
set -uo pipefail

profile="host"; use_docker=0; targets="twatch_ultra"
while [ $# -gt 0 ]; do case "$1" in
  --host) profile="host";; --full) profile="full";;
  --docker) use_docker=1;; --targets) shift; targets="${1:-}";;
  *) echo "unknown argument: $1" >&2; exit 2;; esac; shift; done
IFS=',' read -r -a TARGETS <<< "$targets"

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

# Source identity: the commit under test. CI supplies GITHUB_SHA. Locally the
# commit is read straight from .git without running git, so the gate never
# takes a git lock on a working checkout. Uncommitted edits are NOT captured.
source_identity() {
  if [ -n "${GITHUB_SHA:-}" ]; then echo "$GITHUB_SHA (CI checkout)"; return; fi
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
  echo "docker        = $([ $use_docker -eq 1 ] && echo yes || echo no)"
  echo "host_uname    = $(uname -srm)"
  echo "host_cxx      = $("$CXX" --version 2>/dev/null | head -n1)"
  echo "source_identity = $(source_identity)"
  echo "sensors_identity = $(sensors_identity)"
  echo "# device dependency pins (not upgraded by the gate):"
  grep -hE '^\s*(https://|[a-z0-9_-]+/[A-Za-z0-9_-]+ @|platform = )' "$root/devices/lilygo-tultra/platformio.ini" 2>/dev/null | sed 's/^/  dep: /'
  echo "container_digest = $([ $use_docker -eq 1 ] && echo '(set when image is built)' || echo 'n/a (host execution)')"
  echo "parity_note = verdict matches CI only when the inputs above match; same-script alone does not guarantee it"
} > "$manifest"

echo "LayerTime pre-flight"
echo "profile: $profile    firmware targets: $targets    docker: $([ $use_docker -eq 1 ] && echo yes || echo no)"
echo "inputs recorded in: tools/ci/logs/preflight_manifest.txt"
echo

# ---------------- Tier 0: host checks (both profiles) -----------------------
echo "Sensor library revision:"
# Every host test and device build must use the one library revision LayerTime
# records. "git ls-tree" reads that record without touching the index.
recorded=""; command -v git >/dev/null 2>&1 && recorded="$(git -C "$root" ls-tree HEAD sensors 2>/dev/null | awk '$2=="commit"{print $3}')"
checked="$(sensors_identity | cut -d' ' -f1)"
if [ -z "$recorded" ]; then
  echo "  no recorded submodule revision found (git unavailable, or sensors/ is an in-tree folder); not compared"
elif [ "$checked" = "$recorded" ]; then
  PASS+=("sensors revision ($checked, the recorded one)")
elif [ "${LAYERTIME_SENSORS_UNRECORDED:-}" = "1" ]; then
  echo "  sensors/ is at $checked, LayerTime records $recorded; continuing because LAYERTIME_SENSORS_UNRECORDED=1"
  NOTRUN+=("sensors revision (deliberately unrecorded: $checked, recorded $recorded)")
else
  FAIL+=("sensors revision (sensors/ is at $checked, LayerTime records $recorded; run: git submodule update sensors)")
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
  builder=""; digest=""
  if [ $use_docker -eq 1 ]; then
    if command -v docker >/dev/null 2>&1 && [ -n "${LAYERTIME_IMAGE:-}" ]; then builder="docker"; else
      for t in "${TARGETS[@]}"; do NOTRUN+=("firmware:$t (docker or LAYERTIME_IMAGE unavailable)"); done; fi
  else
    if command -v pio >/dev/null 2>&1 || command -v platformio >/dev/null 2>&1; then builder="native"; else
      for t in "${TARGETS[@]}"; do NOTRUN+=("firmware:$t (pioarduino toolchain not on PATH; use --docker or install it)"); done; fi
  fi
  if [ -n "$builder" ]; then
    for t in "${TARGETS[@]}"; do
      dev="$root/devices/lilygo-tultra"; [ -d "$root/devices/$t" ] && dev="$root/devices/$t"
      # The device project builds from its own folder; respect its pinned platform/libs.
      if ( cd "$dev" && pio run -e "$t" ) >"$out/fw_$t.log" 2>&1; then PASS+=("firmware:$t"); else FAIL+=("firmware:$t (see logs/fw_$t.log)"); cp "$out/fw_$t.log" "$logdir/fw_$t.log" 2>/dev/null; fi
    done
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

if [ "${#FAIL[@]}" -eq 0 ] && [ "$required_notrun" -eq 0 ]; then
  if [ "$profile" = "host" ]; then echo "HOST PROFILE PASSED: host checks passed. Firmware build NOT RUN (this does not certify a firmware build)."
  else echo "FULL PROFILE PASSED: host checks and required firmware targets built."; fi
  exit 0
else
  [ "$required_notrun" -eq 1 ] && echo "PROFILE FAILED: a required firmware target could not run (NOT RUN counts as failure for --full)."
  [ "${#FAIL[@]}" -ne 0 ] && echo "PROFILE FAILED: ${#FAIL[@]} check(s) failed."
  exit 1
fi
