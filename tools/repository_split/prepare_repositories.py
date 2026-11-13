"""Prepare non-destructive, local top-level product/library repository copies.

Only tracked product inputs and explicitly named extracted libraries are copied.
Never copies private captures, signing keys, models, build caches, or .git.
Refuses every existing output directory. Does not stage, commit or publish.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
NAMES = ("LayerTime-Core", "LayerTime-C5", "LayerTime-Sensors", "LayerTime-Contracts", "Hunter", "Vixen")


def git(*args, root=ROOT):
    return subprocess.check_output(["git", "--no-optional-locks", "-c", f"safe.directory={root.as_posix()}",
                                    "-C", str(root), *args], text=True).strip()


def copy(source, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, dest)


def tracked_tree(prefix, destination, excluded=()):
    for rel in git("ls-files", "-z", prefix).split("\0"):
        if not rel or not (ROOT / rel).is_file():
            continue
        local = Path(rel).relative_to(prefix)
        if any(part in excluded for part in local.parts):
            continue
        copy(ROOT / rel, destination / local)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--initialize", action="store_true", help="Initialize empty local Git repositories; no commits.")
    args = parser.parse_args()
    dest = args.destination.resolve()
    for name in NAMES:
        if (dest / name).exists():
            raise SystemExit(f"Refusing existing destination: {dest / name}")
    dest.mkdir(parents=True, exist_ok=True)
    provenance = {"sourceRepository": "https://github.com/mikevan/LayerTime.git",
                  "sourceCommit": git("rev-parse", "HEAD"),
                  "content": "Current working-tree extraction; not a claim of a new committed release.",
                  "originalHistoryRetainedAt": str(ROOT), "published": False}
    for name in NAMES:
        (dest / name).mkdir()
    for name in ("LayerTime-Core", "LayerTime-C5"):
        for source in (ROOT / "libraries" / name).rglob("*"):
            relative = source.relative_to(ROOT / "libraries" / name)
            if source.is_file() and relative.parts[0] in {"src", "library.json", "README.md", "LICENSE", "CMakeLists.txt"}:
                copy(source, dest / name / source.relative_to(ROOT / "libraries" / name))
    integration = {"test_boundary", "test_core_model", "test_link_vectors_mc", "test_sensor_equivalence"}
    for source in (ROOT / "test").glob("*.h"):
        copy(source, dest / "LayerTime-Core/test" / source.name)
    for source in (ROOT / "test").glob("test_*/*.cpp"):
        if source.parent.name not in integration:
            copy(source, dest / "LayerTime-Core/test" / source.relative_to(ROOT / "test"))
    sensors = ROOT / "sensors"
    for rel in git("ls-files", "-z", root=sensors).split("\0"):
        if rel and (sensors / rel).is_file():
            copy(sensors / rel, dest / "LayerTime-Sensors" / rel)
    tracked_tree("contracts", dest / "LayerTime-Contracts")
    copy(ROOT / "LICENSE", dest / "LayerTime-Contracts/LICENSE")

    hunter = dest / "Hunter"
    tracked_tree("devices/lilygo-hunter", hunter)
    for name in ("README.md", "library.json"):
        assert (dest / "LayerTime-Core" / name).is_file()
    # Shared files have already left the device folder; tracked_tree skips them.
    for name in ("hunter-logo.png", "vixen-logo.png", "LayerTime-owl.svg"):
        copy(ROOT / "assets" / name, hunter / "assets" / name)
    copy(ROOT / "LICENSE", hunter / "LICENSE")
    config = (hunter / "platformio.ini").read_text(encoding="utf-8")
    config = config.replace("src_dir = ../..", "src_dir = src")
    config = config.replace("+<devices/lilygo-hunter/src/>", "+<*>")
    config = config.replace("-I ../../libraries/LayerTime-Core/src", "-I ../LayerTime-Core/src")
    config = config.replace("pre:../../tools/sensors_check.py", "pre:tools/dependencies_check.py")
    config = config.replace("../../libraries/LayerTime-", "../LayerTime-")
    config = config.replace("../../sensors", "../LayerTime-Sensors")
    (hunter / "platformio.ini").write_text(config, newline="\n", encoding="utf-8")
    copy(HERE / "dependencies_check.py", hunter / "tools/dependencies_check.py")
    copy(HERE / "hunter_host_tests.py", hunter / "test/run_host_tests.py")
    spec = importlib.util.spec_from_file_location("dependency_guard", HERE / "dependencies_check.py")
    guard = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guard)
    lock = {"formatVersion": 1, "libraries": {}}
    for name in ("LayerTime-Core", "LayerTime-C5", "LayerTime-Sensors"):
        library = dest / name
        files = list((library / "src").rglob("*")) + [library / "library.json"]
        lock["libraries"][name] = {"path": "../" + name,
            "files": {p.relative_to(library).as_posix(): guard.fingerprint(p) for p in files if p.is_file()}}
    (hunter / "dependencies.lock.json").write_text(json.dumps(lock, indent=2) + "\n", encoding="utf-8")
    guard.check(hunter)
    # Retain serial-number targeting and the port-in-use refusal in flash tools.
    for p in (hunter / "tools").glob("*.ps1"):
        text = p.read_text(encoding="utf-8")
        text = text.replace("'C:\\workspace\\TUltra-Project\\LayerTime'", "(Split-Path -Parent $PSScriptRoot)")
        text = text.replace("Join-Path $RepoDir 'devices\\lilygo-hunter'", "$RepoDir")
        p.write_text(text, encoding="utf-8")
    generator = hunter / "tools/c5_logo_images.py"
    text = generator.read_text(encoding="utf-8").replace(".parents[3]", ".parents[1]")
    text = text.replace('ROOT / "libraries/LayerTime-C5/src/C5LogoImages.cpp"', 'ROOT.parent / "LayerTime-C5/src/C5LogoImages.cpp"')
    generator.write_text(text, encoding="utf-8")

    vixen = dest / "Vixen"
    tracked_tree("devices/layertime-controller-android", vixen / "android", excluded=("releases",))
    # Preserve tracked teaching assets and frozen evidence for reproducibility.
    # Untracked external data, local model weights and training runs are excluded.
    tracked_tree("tools/ai-training", vixen / "tools/ai-training")
    tracked_tree("tools/ai", vixen / "tools/ai")
    tracked_tree("third_party", vixen / "third_party")
    tracked_tree("docs/ai-development", vixen / "docs/ai-development")
    copy(ROOT / "LICENSE", vixen / "LICENSE")
    copy(ROOT / ".gitattributes", vixen / ".gitattributes")
    cmake = vixen / "android/app/src/main/cpp/CMakeLists.txt"
    cmake.write_text(cmake.read_text(encoding="utf-8").replace("../../../../../../third_party", "../../../../../third_party"), encoding="utf-8")
    settings = vixen / "android/settings.gradle.kts"
    settings.write_text(settings.read_text(encoding="utf-8").replace('rootProject.name = "LayerTimeController"', 'rootProject.name = "Vixen"'), encoding="utf-8")
    # Rename display text consistently, including notifications and exports.
    # Lowercase Android application IDs, package names and storage keys remain
    # unchanged for compatibility. Update Kotlin symbol references together.
    for p in (vixen / "android/app/src").rglob("*"):
        if p.is_file() and p.suffix in {".kt", ".xml"}:
            text = p.read_bytes()
            updated = text.replace(b"LayerScope", b"Vixen")
            if text != updated:
                p.write_bytes(updated)
    for p in (vixen / "tools").rglob("*"):
        if p.is_file() and p.suffix in {".py", ".ps1"} and len(p.relative_to(vixen / "tools").parts) <= 3:
            text = p.read_text(encoding="utf-8")
            p.write_text(text.replace("devices/layertime-controller-android", "android"), encoding="utf-8")

    descriptions = {
        "Hunter": "LilyGO T-Dongle-C5 sensor, SD logging, PCAP evidence and card-transfer product. The Hunter logo is unchanged.",
        "Vixen": "Flagship Android analysis product, formerly LayerScope. Android installation IDs and Kotlin package names are preserved for update compatibility.",
        "LayerTime-Contracts": "Canonical application and Link wire contracts and test vectors for the C++, Kotlin and Monkey C bindings.",
    }
    for name in NAMES:
        repo = dest / name
        ignore = repo / ".gitignore"
        existing = ignore.read_text(encoding="utf-8") if ignore.exists() else ""
        ignore.write_text(existing + "\n.pio/\nbin/\nbuild/\n__pycache__/\n*.log\n.env\n*.jks\nkeystore.properties\n.gradle/\n.kotlin/\n.ndk-cache/\nthird_party/llama.cpp/\n", encoding="utf-8")
        (repo / "EXTRACTION.json").write_text(json.dumps(dict(provenance, project=name,
            sensorsRevision=git("rev-parse", "HEAD", root=sensors)), indent=2) + "\n", encoding="utf-8")
        if name in descriptions:
            original = repo / "README.md"
            if original.exists():
                copy(original, repo / "docs/legacy-readme.md")
            intro = f"# {name}\n\n{descriptions[name]}\n\n"
            if name == "Hunter":
                intro += ("Keep LayerTime-Core, LayerTime-C5 and LayerTime-Sensors as sibling checkouts. "
                          "Run `python tools/dependencies_check.py` before building. The build repeats this content-lock check.\n\n"
                          "Run native host checks with `python test/run_host_tests.py` using a C++17 compiler.\n\n"
                          "Build with `pio run -e tdongle_c5_hunter`. No hardware is flashed by a build. "
                          "The legacy `tdongle_c5_vixen` environment is the old fox-hunt beacon, not the Vixen flagship.\n")
            if name == "Vixen":
                intro += ("Android project: `android/`. Product AI workflow and tracked teaching/evaluation assets: `tools/ai-training/`. "
                          "Notella remains a separate training product. Native inference is pinned by `third_party/llama.cpp.lock.json`; "
                          "restore it with `tools/ai/fetch-runtime.ps1`. Wireshark native libraries must be rebuilt/staged using the Android build tooling.\n\n"
                          "Protocol reference: sibling LayerTime-Contracts. Hunter firmware and watch firmware are independent products. "
                          "This source extraction is not a device-verified release. Local models, signing keys, "
                          "private captures, old APK releases and build caches were not copied.\n")
            if name == "LayerTime-Contracts":
                intro += ("Read [models](models.md), [commands](commands.md), [capabilities](capabilities.md), "
                          "[Link 0.1](link.md), [Link 0.2](link_0_2.md), and the canonical [vectors](vectors/). "
                          "These are common definitions and test data, rather than a compiled code library. "
                          "Core codec tests use LAYERTIME_CONTRACTS_DIR; Vixen's protocol tests accept the same environment variable.\n")
            original.write_text(intro + "\nSource ancestry is recorded in `EXTRACTION.json`; original Git history remains in LayerTime. No publication occurred.\n", encoding="utf-8")
        if args.initialize:
            subprocess.run(["git", "init", "-b", "main", str(repo)], check=True, capture_output=True)
    print(json.dumps({"destination": str(dest), "repositories": list(NAMES), "initialized": args.initialize,
                      "committed": False, "published": False}, indent=2))


if __name__ == "__main__":
    main()
