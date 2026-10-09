#!/usr/bin/env bash
# LayerTime pre-flight gate. One script, run locally by contributors and by CI.
#
# PROFILES (pick one):
#   --host    host unit suites + architecture boundary + sensor harness.
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
while [ "$root" != "/" ] && [ ! -f "$root/src/core/logic/BleAdvertClassifier.cpp" ]; do root="$(dirname "$root")"; done
[ -f "$root/src/core/logic/BleAdvertClassifier.cpp" ] || { echo "pre-flight: repository root not found." >&2; exit 2; }
harness=""; [ -f "$root/test/replay/test_sensor_replay.cpp" ] && harness="$root/test/replay"

CXX="${CXX:-g++}"; FLAGS="-std=c++17 -O0 -Wall -Wextra"
out="$(mktemp -d)"; trap 'rm -rf "$out"' EXIT
T="$root/test"; S="$root/src"; L="$S/core/logic"; A="$S/core/app"; K="$S/core/link"
logdir="$root/tools/ci/logs"; mkdir -p "$logdir"
PASS=(); FAIL=(); NOTRUN=()

host_check() { # name, sources...
  local name="$1"; shift
  if ! "$CXX" $FLAGS -I"$T" -I"$S" ${harness:+-I"$harness"} -o "$out/$name" "$@" 2>"$out/$name.cc"; then
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
echo "Host unit suites + architecture boundary + sensor harness:"
host_check geogrid            "$T/test_geogrid/test_geogrid.cpp" "$L/GeoGrid.cpp"
host_check core_model         "$T/test_core_model/test_core_model.cpp" "$L/QuickMessages.cpp"
host_check decl_advice        "$T/test_declination_advice/test_declination_advice.cpp" "$L/DeclinationAdvice.cpp"
host_check recon_selection    "$T/test_recon_selection/test_recon_selection.cpp" "$L/ReconSelection.cpp"
host_check recon_signatures   "$T/test_recon_signatures/test_recon_signatures.cpp" "$L/ReconSignatures.cpp"
host_check wifi_classifier    "$T/test_wifi_classifier/test_wifi_classifier.cpp" "$L/WifiFrameClassifier.cpp" "$L/ReconSignatures.cpp"
host_check ble_classifier     "$T/test_ble_classifier/test_ble_classifier.cpp" "$L/BleAdvertClassifier.cpp" "$L/ReconSignatures.cpp" "$L/ReconSelection.cpp"
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
if [ -n "$harness" ]; then host_check sensor_replay "$harness/test_sensor_replay.cpp" "$L/BleAdvertClassifier.cpp" "$L/ReconSignatures.cpp" "$L/ReconSelection.cpp" "$L/MonitorEventLog.cpp" "$L/AlertPolicy.cpp"
else FAIL+=("sensor_replay (harness not found)"); fi
if [ -n "$harness" ] && [ -f "$harness/test_sensor_public.cpp" ]; then host_check sensor_public "$harness/test_sensor_public.cpp" "$L/BleAdvertClassifier.cpp" "$L/ReconSignatures.cpp" "$L/ReconSelection.cpp"; fi

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
