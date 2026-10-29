#!/usr/bin/env bash
# Verify the packaged arm64 native set loads on the target before building an APK.
# 1) file closure: every DT_NEEDED is packaged or a platform library
# 2) symbol closure: every strong undefined dynamic symbol is exported by the
#    packaged set or by the platform libraries at the target API level
# Usage: check_native_closure.sh <jniLibs/arm64-v8a> <NDK> [API=34] [extra .so ...]
set -uo pipefail
J="$1"; NDK="$2"; API="${3:-34}"; shift 3 2>/dev/null || shift $#
TC="$NDK/toolchains/llvm/prebuilt/linux-x86_64"; NM="$TC/bin/llvm-nm"; RE="$TC/bin/llvm-readelf"
SYS="$TC/sysroot/usr/lib/aarch64-linux-android/$API"
PLATFORM="libc.so libm.so libdl.so liblog.so libandroid.so"
LIBS=( "$J"/lib*.so "$@" ); W=$(mktemp -d); rc=0
for f in "${LIBS[@]}"; do basename "$f"; done | sort -u > "$W/have"
printf "%s\n" $PLATFORM >> "$W/have"; sort -u -o "$W/have" "$W/have"
echo "== file closure =="
for f in "${LIBS[@]}"; do
  "$RE" -d "$f" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | while read -r n; do
    grep -qx "$n" "$W/have" || echo "MISSING: $n (needed by $(basename "$f"))"; done
done | tee "$W/miss"; [ -s "$W/miss" ] && rc=1 || echo "ok: all DT_NEEDED resolvable"
echo "== symbol closure (API $API) =="
for f in "${LIBS[@]}" "$SYS"/libc.so "$SYS"/libm.so "$SYS"/libdl.so "$SYS"/liblog.so; do
  "$NM" -D --defined-only "$f" 2>/dev/null | awk '{print $NF}'; done | sed 's/@.*//' | sort -u > "$W/exp"
for f in "${LIBS[@]}"; do
  "$NM" -D --undefined-only "$f" | awk '$1=="U"{print $2}' | sed 's/@.*//' | sort -u |
    comm -23 - "$W/exp" | sed "s|^|UNRESOLVED: $(basename "$f") -> |"
done | tee "$W/sym"; [ -s "$W/sym" ] && rc=1 || echo "ok: all strong undefined symbols resolve"
rm -rf "$W"; exit $rc
