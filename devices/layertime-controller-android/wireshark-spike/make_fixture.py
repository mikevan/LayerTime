#!/usr/bin/env python3
# Build a known 802.11 deauthentication fixture as a pcap (DLT_IEEE802_11 = 105),
# no radiotap, matching what a LayerWand promiscuous callback yields (raw 802.11).
import struct, sys

# --- 802.11 deauth management frame ---
# Frame Control: version 0, type 0 (mgmt), subtype 12 (deauth) -> 0xC0, flags 0x00
fc = bytes([0xC0, 0x00])
duration = struct.pack('<H', 0x013A)          # 314 us (typical)
da  = bytes.fromhex('ffffffffffff')            # broadcast (deauth flood target)
sa  = bytes.fromhex('00112233AABB')            # spoofed AP / attacker TA
bssid = bytes.fromhex('00112233AABB')          # AP BSSID
seqctl = struct.pack('<H', (0x07 << 4) | 0)    # seq 7, frag 0
reason = struct.pack('<H', 0x0007)             # reason 7: class 3 frame from nonassociated STA
frame = fc + duration + da + sa + bssid + seqctl + reason
# Append a 4-byte FCS placeholder? Leave off; our capture has no FCS (sig_len excludes it here).

# --- pcap global header (classic) ---
PCAP_MAGIC = 0xa1b2c3d4
ghdr = struct.pack('<IHHiIII', PCAP_MAGIC, 2, 4, 0, 0, 65535, 105)  # linktype 105 = IEEE802_11
# --- record header ---
ts_sec, ts_usec = 1700000000, 123456
rhdr = struct.pack('<IIII', ts_sec, ts_usec, len(frame), len(frame))

with open('deauth_fixture.pcap','wb') as f:
    f.write(ghdr); f.write(rhdr); f.write(frame)

print("wrote deauth_fixture.pcap, frame %d bytes:" % len(frame))
print(frame.hex())
