# Host test for tools/s3plus_upload_guard.py. Run from the repository root:
#     python tools/test_s3plus_upload_guard.py
# No hardware, no SCons: it checks the decisions the guard makes.

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s3plus_upload_guard as g  # noqa: E402

failures = 0
checks = 0


def check(name, condition):
    global failures, checks
    checks += 1
    if not condition:
        failures += 1
        print("FAIL: " + name)


S3_MAC = "AA:BB:CC:00:11:22"
S3_SERIAL = "AA:BB:CC:00:11:22"
BOARDS = {S3_SERIAL: S3_MAC}
PORTS = [("COM5", "38:44:BE:BC:EC:E0"), ("COM7", S3_SERIAL), ("COM9", None)]

# esptool 5.1.0 read-mac output for an ESP32-S3 (esptool/cmds.py read_mac).
OUT_S3 = ("esptool v5.1.0\nConnected to ESP32-S3 on COM7:\n"
          "Chip type:          ESP32-S3 (QFN56) (revision v0.2)\n"
          "MAC:                aa:bb:cc:00:11:22\nHard resetting via RTS pin...\n")
# A chip with an EUI-64 prints an 8-byte MAC line first; it must not parse as a 6-byte MAC.
OUT_EUI64 = "MAC:                60:55:f9:ff:fe:f7:2c:a2\nBASE MAC:           60:55:f9:f7:2c:a2\n"

check("parses the S3 MAC", g.parse_mac(OUT_S3) == S3_MAC)
check("an 8-byte EUI-64 line is not a MAC", g.parse_mac(OUT_EUI64) is None)
check("no MAC line gives None", g.parse_mac("A fatal error occurred") is None)

d, s, e = g.choose_port("", PORTS, BOARDS)
check("auto-selects the registered port", (d, s, e) == ("COM7", S3_SERIAL, None))
d, s, e = g.choose_port("COM5", PORTS, BOARDS)
check("refuses an explicit port that is not registered (a LayerWand)", d is None and "not a registered" in e)
d, s, e = g.choose_port("com7", PORTS, BOARDS)
check("accepts the registered port named explicitly, any case", d == "COM7")
d, s, e = g.choose_port("COM3", PORTS, BOARDS)
check("refuses a port that does not exist", d is None and "not found" in e)
d, s, e = g.choose_port("", [("COM5", "38:44:BE:BC:EC:E0")], BOARDS)
check("refuses when no registered board is connected", d is None and "No registered" in e)
d, s, e = g.choose_port("", PORTS + [("COM8", "11:22")], {S3_SERIAL: S3_MAC, "11:22": "11:22:33:44:55:66"})
check("refuses when two registered boards are connected", d is None and "More than one" in e)
d, s, e = g.choose_port("", PORTS, {})
check("refuses everything with an empty allowlist", d is None)

with tempfile.TemporaryDirectory() as tmp:
    path = os.path.join(tmp, "allowed.txt")
    check("a missing allowlist file is empty", g.load_allowlist(path) == {})
    with open(path, "w", encoding="utf-8") as f:
        f.write("# comment\n\naa:bb:cc:00:11:22 SER1\nnot-a-mac SER2\nAA:BB:CC:00:11:23 SER3 extra\n")
    loaded = g.load_allowlist(path)
    check("loads valid lines and skips bad ones", loaded == {"SER1": "AA:BB:CC:00:11:22"})

print("%d checks, %d failed" % (checks, failures))
sys.exit(1 if failures else 0)
