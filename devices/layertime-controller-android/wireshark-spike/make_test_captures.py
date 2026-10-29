#!/usr/bin/env python3
"""Write the synthetic capture-import test files (classic pcap, DLT 105 raw 802.11).

lt_test_5frames.pcap   - the five frames of WiresharkDecoder.TEST_FRAMES, record i
                         stamped 1700000000+i s. Byte-identical to
                         WiresharkDecoder.pcapBytes(TEST_FRAMES) (checked by a unit test).
lt_test_bulk_3000.pcap - the same five frames repeated to 3,000 records, to exercise
                         progress reporting, the 500-frame display limit, and cancel.
Synthetic test data, not captured traffic.
Usage: python3 make_test_captures.py [output_dir]
"""
import struct, sys, os

def h(s): return bytes.fromhex(s.replace(" ", ""))

DEAUTH = h("c0003a01 ffffffffffff 00112233aabb 00112233aabb 7000 0700")
FRAMES = [
    DEAUTH,
    h("a0003a01 001122334455 00112233aabb 00112233aabb 8000 0800"),
    h("80000000 ffffffffffff 00112233aabb 00112233aabb 9000 0000000000000000 6400 0104 00074c542d54455354 010182 030106"),
    h("40000000 ffffffffffff 020000000001 ffffffffffff a000 0000 010182"),
    DEAUTH[:20],
]

def pcap(frames):
    out = struct.pack("<IHHiIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 105)
    for i, f in enumerate(frames):
        out += struct.pack("<IIII", 1700000000 + i, 0, len(f), len(f)) + f
    return out

d = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
for name, frames in (("lt_test_5frames.pcap", FRAMES),
                     ("lt_test_bulk_3000.pcap", [FRAMES[i % 5] for i in range(3000)])):
    p = os.path.join(d, name)
    with open(p, "wb") as fh: fh.write(pcap(frames))
    print(name, os.path.getsize(p), "bytes")
