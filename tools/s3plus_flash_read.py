# LayerTime, T-Watch S3 Plus target: read-only flash capture for diagnosis.
#
# Run from s3plus/ with the S3 Plus project's own Python:
#     & "C:\Users\vange\.platformio-s3plus\penv\Scripts\python.exe" ..\tools\s3plus_flash_read.py
#
# It READS flash and writes nothing to the watch. It talks only to the
# registered T-Watch S3 Plus (the port whose USB serial number is in the
# upload guard's list, confirmed by MAC before anything is read), so it can
# never touch the T-Watch Ultra or a LayerWand. Close the serial monitor
# first; the port must be free.
#
# What it captures, into s3plus/bin/flashdump/ (git-ignored):
#   partition_table.bin  0x8000,   0xC00     the partition table on the watch now
#   ffat.bin             0x810000, 0x7E0000  the whole FFat partition as flashed
#   factory_fat_head.bin 0x610000, 0x10000   where LilyGo's other 16 MB layout
#                                            (3 MB app / 9.9 MB FAT, the board's
#                                            default) starts its FAT partition;
#                                            LayerTime never writes this range
#   SHA256SUMS.txt       a hash of each file
#
# The watch resets into download mode for the read and is reset again after.

import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import s3plus_upload_guard as guard  # noqa: E402

from serial.tools import list_ports  # noqa: E402

OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "s3plus", "bin", "flashdump"))
REGIONS = [
    ("partition_table.bin", 0x8000, 0xC00),
    ("ffat.bin", 0x810000, 0x7E0000),
    ("factory_fat_head.bin", 0x610000, 0x10000),
]


def esptool():
    scripts = os.path.dirname(sys.executable)
    for name in ("esptool.exe", "esptool"):
        path = os.path.join(scripts, name)
        if os.path.isfile(path):
            return [path]
    return [sys.executable, "-m", "esptool"]


def main():
    boards = guard.load_allowlist()
    if not boards:
        print("No T-Watch S3 Plus is registered. Nothing was read.")
        return 1
    ports = [(p.device, p.serial_number) for p in list_ports.comports()]
    device, serial, error = guard.choose_port("", ports, boards)
    if error:
        print(error + " Nothing was read.")
        return 1
    base = esptool() + ["--chip", "esp32s3", "--port", device, "--baud", "921600"]
    probe = subprocess.run(base + ["--before", "default-reset", "--after", "no-reset", "read-mac"],
                           capture_output=True, text=True, check=False)
    mac = guard.parse_mac(probe.stdout + "\n" + probe.stderr)
    if mac != boards[serial]:
        print(probe.stdout + probe.stderr)
        print("The board on %s did not report the registered MAC. Nothing was read." % device)
        return 1
    print("Reading from the registered T-Watch S3 Plus on %s (MAC %s)." % (device, mac))
    os.makedirs(OUT_DIR, exist_ok=True)
    sums = []
    for i, (name, address, size) in enumerate(REGIONS):
        path = os.path.join(OUT_DIR, name)
        after = "hard-reset" if i == len(REGIONS) - 1 else "no-reset"
        cmd = base + ["--before", "default-reset", "--after", after, "read-flash",
                      hex(address), hex(size), path]
        print("Reading %s: 0x%06X, %d bytes." % (name, address, size))
        result = subprocess.run(cmd, check=False)
        if result.returncode != 0 or not os.path.isfile(path) or os.path.getsize(path) != size:
            print("Reading %s failed. Stopped." % name)
            return 1
        with open(path, "rb") as handle:
            sums.append("%s  %s" % (hashlib.sha256(handle.read()).hexdigest(), name))
    with open(os.path.join(OUT_DIR, "SHA256SUMS.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(sums) + "\n")
    print("Done. Files are in %s. Nothing was written to the watch." % OUT_DIR)
    return 0


if __name__ == "__main__":
    sys.exit(main())
