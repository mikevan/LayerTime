"""Validate accepted downloads before adding them to the Pages build."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct


APK_ID = "com.projectrevivesolutions.layerscope"
APK_CERT_SHA256 = "907dfd514ed86f34f3caba164a6df00d78fd70bca3479b895c0e9798370747f0"
ESP32_C5_CHIP_ID = 23


def verify_revision(directory, expected):
    actual = (directory / "source.txt").read_text(encoding="utf-8").strip()
    if not re.fullmatch("[0-9a-f]{40}", expected) or actual != expected:
        raise ValueError(f"Unexpected source revision in {directory}")


def verify_factory_image(path):
    with path.open("rb") as image:
        for offset in (0x2000, 0x10000):
            image.seek(offset)
            header = image.read(24)
            if (len(header) != 24 or header[0] != 0xE9
                    or struct.unpack_from("<H", header, 12)[0] != ESP32_C5_CHIP_ID):
                raise ValueError(f"{path.name} lacks an ESP32-C5 image at {offset:#x}")
        image.seek(0x8000)
        if image.read(2) != b"\xaa\x50":
            raise ValueError(f"{path.name} lacks a factory partition table")


def verify_apk(directory):
    certificate_report = (directory / "certificates.txt").read_text(encoding="utf-8")
    digests = re.findall(r"^Signer #\d+ certificate SHA-256 digest: ([0-9a-f]+)$",
                         certificate_report, re.MULTILINE)
    if digests != [APK_CERT_SHA256]:
        raise ValueError("APK signing certificate differs from the previous public download")

    signed = read_package(directory / "package.txt")
    if signed[0] != APK_ID or signed[1] < 84:
        raise ValueError("APK must use the existing public package ID and a compatible version code")
    if signed != read_package(directory / "ci-package.txt"):
        raise ValueError("Locally signed APK package or version differs from the tested CI build")
    return signed[1], signed[2]


def read_package(path):
    package = re.search(r"^package: name='([^']+)' versionCode='(\d+)' versionName='([^']+)'",
                        path.read_text(encoding="utf-8"), re.MULTILINE)
    if not package:
        raise ValueError(f"Missing APK package metadata in {path.name}")
    return package[1], int(package[2]), package[3]


def stage(site, build, hunter_rev, vixen_rev, site_rev):
    hunter = build / "hunter"
    vixen = build / "vixen"
    verify_revision(hunter, hunter_rev)
    verify_revision(vixen, vixen_rev)
    version_code, version_name = verify_apk(vixen)
    previous = build / "previous.json"
    if previous.exists():
        old_version = json.loads(previous.read_text(encoding="utf-8"))["android"]["versionCode"]
        if version_code < old_version:
            raise ValueError("APK version code would downgrade the current public download")
    artifacts = {
        "firmware/hunter.bin": hunter / "hunter.bin",
        "firmware/vixen.bin": hunter / "vixen.bin",
        "layerscope.apk": vixen / "layerscope.apk",
    }
    for filename in ("firmware/hunter.bin", "firmware/vixen.bin"):
        verify_factory_image(artifacts[filename])
    if artifacts["firmware/hunter.bin"].read_bytes() == artifacts["firmware/vixen.bin"].read_bytes():
        raise ValueError("Hunter and Vixen factory images are identical")

    metadata = {
        "site": site_rev,
        "hunter": hunter_rev,
        "vixen": vixen_rev,
        "android": {"versionCode": version_code, "versionName": version_name},
        "files": {},
    }
    for name, source in artifacts.items():
        with source.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        metadata["files"][name] = {"bytes": source.stat().st_size, "sha256": digest}

    for name, source in artifacts.items():
        destination = site / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
    for name in ("hunter", "vixen"):
        manifest = site / f"{name}.json"
        settings = json.loads(manifest.read_text(encoding="utf-8"))
        settings["version"] = hunter_rev[:12]
        manifest.write_text(json.dumps(settings, indent=2) + "\n", encoding="utf-8")
    (site / "downloads.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    parser.add_argument("build", type=Path)
    parser.add_argument("hunter_revision")
    parser.add_argument("vixen_revision")
    parser.add_argument("site_revision")
    args = parser.parse_args()
    stage(args.site, args.build, args.hunter_revision, args.vixen_revision, args.site_revision)
