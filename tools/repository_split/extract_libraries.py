"""One-time, fail-closed extraction of unchanged core and shared C5 sources.

Moves only the explicit source sets below inside this checkout. Refuses to
overwrite a destination. Does not touch Git metadata or the sensors submodule.
"""
from pathlib import Path
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "libraries/LayerTime-Core"
C5 = ROOT / "libraries/LayerTime-C5"
BOARD_FILES = ["BringUpLogic.cpp", "BringUpLogic.h", "C5Display.cpp", "C5Display.h",
               "C5Led.cpp", "C5Led.h", "C5LogoImages.cpp", "C5LogoImages.h", "TDongleC5Pins.h"]


def main():
    if CORE.exists() or C5.exists():
        raise SystemExit("Library destination already exists; extraction was not started.")
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
    moves = [(ROOT / "src/core", CORE / "src/core")]
    moves += [(ROOT / "devices/lilygo-hunter/src" / n, C5 / "src" / n) for n in BOARD_FILES]
    for old, new in moves:
        if not old.exists() or new.exists():
            raise SystemExit(f"Unsafe move: {old} -> {new}")
        # Validate resolved absolute paths before any recursive move.
        old.resolve().relative_to(ROOT.resolve())
        new.resolve().relative_to(ROOT.resolve())
    for old, new in moves:
        new.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(old), str(new))

    # Update live source, build/test tooling and instructional Markdown. Do not
    # rewrite frozen AI experiment sources, datasets, captures or vendor trees.
    selected = [p for p in tracked if p and not any(x in p for x in
                ("/ai-training/", "/ai-development/", "/libdeps/", "/pioarduino-platform-", "third_party/"))]
    selected += [p.relative_to(ROOT).as_posix() for base in (CORE, C5) for p in base.rglob("*") if p.is_file()]
    for rel in selected:
        p = ROOT / rel
        if not p.is_file() or p.suffix not in {".md", ".cpp", ".h", ".py", ".sh", ".ps1", ".ini"}:
            continue
        raw = p.read_bytes()
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            continue
        updated = text.replace("src/core/", "libraries/LayerTime-Core/src/core/")
        # Include-search roots, distinct from source file references.
        for old, new in [('-I../../../src', '-I../../../libraries/LayerTime-Core/src'),
                         ('-I../../src', '-I../../libraries/LayerTime-Core/src'),
                         ('-I../src', '-I../libraries/LayerTime-Core/src'),
                         ('-I ../../src', '-I ../../libraries/LayerTime-Core/src'),
                         ('"$root/src"', '"$root/libraries/LayerTime-Core/src"')]:
            # The ../src used in a device's own test is device source, not core.
            if old == '-I../src' and rel.startswith('devices/'):
                continue
            updated = updated.replace(old, new)
        if rel == "test/test_boundary/test_boundary.cpp":
            updated = updated.replace('"../src"', '"../libraries/LayerTime-Core/src"')
        if rel == "devices/lilygo-hunter/tools/c5_logo_images.py":
            updated = updated.replace("devices/lilygo-hunter/src/C5LogoImages.cpp", "libraries/LayerTime-C5/src/C5LogoImages.cpp")
        if rel == "devices/lilygo-hunter/README.md":
            updated = updated.replace('-I../src', '-I../src -I../../../libraries/LayerTime-C5/src')
            updated = updated.replace('../src/BringUpLogic.cpp', '../../../libraries/LayerTime-C5/src/BringUpLogic.cpp')
        if updated != text:
            p.write_bytes(updated.encode("utf-8"))

    # Libraries are compiled by the dependency manager, not also as app sources.
    for target in ("lilygo-hunter", "lilygo-tultra", "lilygo-s3plus"):
        p = ROOT / "devices" / target / "platformio.ini"
        text = p.read_text()
        text = text.replace("    +<libraries/LayerTime-Core/src/core/>\n", "")
        core_dep = "    symlink://${PROJECT_DIR}/../../libraries/LayerTime-Core\n"
        text = text.replace("    symlink://${PROJECT_DIR}/../../sensors\n", "    symlink://${PROJECT_DIR}/../../sensors\n" + core_dep)
        if target == "lilygo-hunter":
            text = text.replace(core_dep, core_dep + "    symlink://${PROJECT_DIR}/../../libraries/LayerTime-C5\n")
        p.write_text(text, newline="\n")
    for folder, name, description in [
        (CORE, "LayerTime-Core", "Hardware-independent application model, logic, ports and Link protocol."),
        (C5, "LayerTime-C5", "Shared LilyGO T-Dongle-C5 board pins, display, LED and button/identity logic.")]:
        manifest = dict(name=name, version="0.1.0", description=description,
                        license="GPL-3.0-or-later", build=dict(srcDir="src", includeDir="src"),
                        export=dict(include=["src", "library.json", "README.md", "LICENSE"]))
        (folder / "library.json").write_text(json.dumps(manifest, indent=2) + "\n")
        shutil.copyfile(ROOT / "LICENSE", folder / "LICENSE")
    print("Extracted 64 core files and 9 shared C5 files; build and host-test paths updated.")


if __name__ == "__main__":
    main()
