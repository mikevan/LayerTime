# LayerTime, T-Watch S3 Plus target: register the watch for uploads.
#
# Run once, before the first S3 Plus upload, from s3plus/, with the S3 Plus
# project's own Python (created by its first build in ~/.platformio-s3plus):
#     & "$env:USERPROFILE\.platformio-s3plus\penv\Scripts\python.exe" ..\tools\s3plus_register_mac.py
#
# It waits for a NEW serial port to appear, so it only ever talks to the
# board you plug in during the run. It never opens, resets, or probes any
# port that was already connected (a LayerWand that is logging, or the
# T-Watch Ultra). On the new port it reads the chip MAC with esptool,
# confirms the chip is an ESP32-S3, and records "<MAC> <USB serial number>"
# in %USERPROFILE%\.layertime\s3plus_allowed_boards.txt, the file the upload
# guard (tools/s3plus_upload_guard.py) reads. Nothing is written to the board.
#
# Plug the watch in while holding its BOOT button (download mode), so the
# USB serial number recorded is the one the chip's own USB controller
# reports, which is also what LayerTime firmware reports after it is flashed.

import os
import re
import subprocess
import sys
import time

from serial.tools import list_ports

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import s3plus_upload_guard as guard  # noqa: E402

WAIT_SECONDS = 90
CHIP_RE = re.compile(r"ESP32-S3", re.IGNORECASE)


def esptool_path():
    scripts = os.path.dirname(sys.executable)
    for name in ("esptool.exe", "esptool"):
        candidate = os.path.join(scripts, name)
        if os.path.isfile(candidate):
            return [candidate]
    return [sys.executable, "-m", "esptool"]


def snapshot():
    return {p.device: p for p in list_ports.comports()}


def main():
    before = snapshot()
    print("Connected serial ports now: " + (", ".join(sorted(before)) or "none") + ".")
    print("Hold the BOOT button on the T-Watch S3 Plus and plug it in now. "
          "Waiting up to %d seconds for a new port." % WAIT_SECONDS)
    deadline = time.time() + WAIT_SECONDS
    new_port = None
    while time.time() < deadline:
        time.sleep(0.5)
        added = [p for d, p in snapshot().items() if d not in before]
        if len(added) == 1:
            new_port = added[0]
            break
        if len(added) > 1:
            print("More than one new port appeared. Connect only the S3 Plus and run this again.")
            return 1
    if new_port is None:
        print("No new port appeared. Nothing was registered.")
        return 1
    print("New port: %s (USB serial %s)." % (new_port.device, new_port.serial_number))
    if not new_port.serial_number:
        print("That port reports no USB serial number. Nothing was registered.")
        return 1
    time.sleep(1.0)
    command = esptool_path() + ["--chip", "esp32s3", "--port", new_port.device,
                                "--before", "default-reset", "--after", "no-reset", "read-mac"]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    output = result.stdout + "\n" + result.stderr
    mac = guard.parse_mac(output)
    if result.returncode != 0 or mac is None or not CHIP_RE.search(output):
        print(output)
        print("esptool did not identify an ESP32-S3 on that port. Nothing was registered.")
        return 1
    boards = guard.load_allowlist()
    if new_port.serial_number in boards and boards[new_port.serial_number] != mac:
        print("USB serial %s is already registered to MAC %s, not %s. Nothing was changed."
              % (new_port.serial_number, boards[new_port.serial_number], mac))
        return 1
    if boards.get(new_port.serial_number) == mac:
        print("Already registered: MAC %s, USB serial %s." % (mac, new_port.serial_number))
        return 0
    os.makedirs(os.path.dirname(guard.ALLOW_FILE), exist_ok=True)
    with open(guard.ALLOW_FILE, "a", encoding="utf-8") as handle:
        handle.write("%s %s\n" % (mac, new_port.serial_number))
    print("Registered T-Watch S3 Plus: MAC %s, USB serial %s, in %s."
          % (mac, new_port.serial_number, guard.ALLOW_FILE))
    return 0


if __name__ == "__main__":
    sys.exit(main())
