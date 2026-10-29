#!/usr/bin/env bash
# Reproduce the host decoding experiment WITHOUT sudo, using the real Wireshark.
# Proven on an Ubuntu 22.04 x86_64 Linux host (the Cowork device VM).
# It: fetches the tshark 3.6.2 package closure as .debs, extracts them into a
# user prefix, builds the libwireshark embedding harness, and decodes the
# deauth fixture two ways (tshark + harness) for field-by-field comparison.
set -euo pipefail
W="${1:-$HOME/wsspike}"; mkdir -p "$W/debs" "$W/root"; cd "$W"
# 1. closure of tshark runtime deps (apt lists already present; no sudo needed)
apt-cache depends --recurse --no-recommends --no-suggests --no-conflicts \
  --no-breaks --no-replaces --no-enhances tshark 2>/dev/null \
  | grep -E '^[a-z0-9]' | sort -u > want.txt
printf 'tshark\nlibwireshark-dev\nlibwiretap-dev\nlibwsutil-dev\n' >> want.txt
# 2. download (apt-get download needs no root) and extract into ./root
cd debs; while read -r p; do ls ${p}_*.deb >/dev/null 2>&1 || apt-get download "$p" 2>/dev/null || true; done < ../want.txt; cd ..
for d in debs/*.deb; do dpkg -x "$d" root/ 2>/dev/null || true; done
# 3. env pointing at the extracted tree
export LD_LIBRARY_PATH="$W/root/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export WIRESHARK_DATA_DIR="$W/root/usr/share/wireshark"
TS="$W/root/usr/bin/tshark"
# 4. fixture + decodes
python3 "$(dirname "$0")/make_fixture.py"
"$TS" --version | head -1
"$TS" -r deauth_fixture.pcap -o 'wlan.check_fcs:FALSE' -T fields \
  -e frame.len -e wlan.fc.type_subtype -e wlan.da -e wlan.sa -e wlan.bssid \
  -e wlan.seq -e wlan.fixed.reason_code
# 5. build + run the libwireshark embedding harness
gcc -O0 -g "$(dirname "$0")/ws_harness.c" -I root/usr/include/wireshark \
  $(pkg-config --cflags glib-2.0) -o ws_harness \
  -L root/usr/lib/x86_64-linux-gnu -Wl,-rpath,"$W/root/usr/lib/x86_64-linux-gnu" \
  -lwireshark -lwsutil -lwiretap $(pkg-config --libs glib-2.0)
./ws_harness
