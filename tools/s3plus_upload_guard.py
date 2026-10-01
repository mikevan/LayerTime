# LayerTime, T-Watch S3 Plus target: upload guard.
#
# Loaded by s3plus/platformio.ini [env:twatch_s3plus] as
# "post:../tools/s3plus_upload_guard.py".
# It refuses to write firmware to any board that is not a registered
# T-Watch S3 Plus, so S3 Plus firmware can never reach the T-Watch Ultra,
# a LayerWand, or any other ESP32 on the desk.
#
# How it decides, before anything is written:
#   1. Registered boards are read from
#      %USERPROFILE%\.layertime\s3plus_allowed_boards.txt, written by
#      tools/s3plus_register_mac.py. One line per board:
#          <MAC> <USB serial number>
#      The file lives outside the repository, so no MAC is committed.
#      No file, or no entry, means no upload.
#   2. The upload port must be a USB serial port whose USB serial number is a
#      registered one. If upload_port is not set, the guard selects that port
#      itself; it never runs esptool against an unregistered port, so it
#      cannot reset a LayerWand or the Ultra by probing it.
#   3. esptool read-mac on that port must return the registered MAC for that
#      serial number. Only then does the normal upload run.
#
# Every refusal stops the build with a message that says what to do.

import os
import re
import subprocess

try:
    Import("env")  # noqa: F821  (SCons)
except NameError:  # imported by a host test or the register script
    env = None

ALLOW_FILE = os.path.join(os.path.expanduser("~"), ".layertime", "s3plus_allowed_boards.txt")
MAC_RE = re.compile(r"([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})")


def _normalise_mac(text):
    return text.strip().upper()


def load_allowlist(path=ALLOW_FILE):
    """Returns {usb_serial: mac}. Lines starting with # are comments."""
    boards = {}
    if not os.path.isfile(path):
        return boards
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 2 or not MAC_RE.fullmatch(parts[0]):
                continue
            boards[parts[1]] = _normalise_mac(parts[0])
    return boards


def parse_mac(esptool_output):
    """The chip MAC from esptool read-mac output, or None."""
    for line in esptool_output.splitlines():
        line = line.strip()
        if line.upper().startswith("MAC:"):
            value = line[4:].strip()
            if MAC_RE.fullmatch(value):
                return _normalise_mac(value)
    return None


def choose_port(requested_port, ports, boards):
    """Pick the upload port.

    ports: list of (device, usb_serial_number or None).
    Returns (device, usb_serial, error_message).
    """
    if requested_port:
        for device, serial in ports:
            if device.lower() == requested_port.lower():
                if serial in boards:
                    return device, serial, None
                return None, None, (
                    "The upload port %s is not a registered T-Watch S3 Plus." % requested_port)
        return None, None, "The upload port %s was not found." % requested_port
    matches = [(device, serial) for device, serial in ports if serial in boards]
    if not matches:
        return None, None, (
            "No registered T-Watch S3 Plus is connected. If it is connected, hold its BOOT "
            "button while plugging it in, then upload again.")
    if len(matches) > 1:
        return None, None, (
            "More than one registered T-Watch S3 Plus is connected. Set upload_port.")
    return matches[0][0], matches[0][1], None


def _refuse(message):
    print("")
    print("S3 PLUS UPLOAD GUARD: upload refused. " + message)
    print("Registered boards: " + ALLOW_FILE)
    print("")
    env.Exit(1)  # noqa: F821


def _list_ports():
    from serial.tools import list_ports  # pyserial ships with the core
    return [(p.device, p.serial_number) for p in list_ports.comports()]


def guard(source, target, env):  # noqa: ARG001  (SCons action signature)
    boards = load_allowlist()
    if not boards:
        _refuse("No T-Watch S3 Plus is registered. Run ..\\tools\\s3plus_register_mac.py from s3plus/ first.")
        return
    device, serial, error = choose_port(env.subst("$UPLOAD_PORT"), _list_ports(), boards)
    if error:
        _refuse(error)
        return
    uploader = env.subst("$UPLOADER").strip('"')
    command = [uploader, "--chip", "esp32s3", "--port", device,
               "--before", "default-reset", "--after", "no-reset", "read-mac"]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    mac = parse_mac(result.stdout + "\n" + result.stderr)
    if mac is None:
        _refuse("Could not read the chip MAC on %s." % device)
        return
    if mac != boards[serial]:
        _refuse("The board on %s reports MAC %s, but USB serial %s is registered as %s."
                % (device, mac, serial, boards[serial]))
        return
    env.Replace(UPLOAD_PORT=device)
    print("S3 PLUS UPLOAD GUARD: %s is the registered T-Watch S3 Plus (MAC %s)." % (device, mac))


if env is not None:
    for _target in ("upload", "uploadfs", "uploadfsota", "erase", "erase_upload"):
        env.AddPreAction(_target, guard)  # noqa: F821
