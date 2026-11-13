"""Read-only source inventory for splitting the LayerTime workspace.

Writes an evidence report only when --output is supplied. Does not initialize,
stage, commit, rename, move, or publish repositories. Reads tracked source rather
than caches, model weights, captures, signing keys, or local build output.
"""
import argparse
import hashlib
import json
import re
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GROUPS = {
    "shared-core": "libraries/LayerTime-Core/",
    "shared-c5": "libraries/LayerTime-C5/",
    "contracts": "contracts/",
    "hunter-and-legacy-beacon": "devices/lilygo-hunter/",
    "vixen-android-candidate": "devices/layertime-controller-android/",
    "watch-ultra": "devices/lilygo-tultra/",
    "watch-s3plus": "devices/lilygo-s3plus/",
    "garmin": "devices/garmin-tactix/",
    "ai-development": "tools/ai-training/",
    "ai-runtime-tools": "tools/ai/",
    "shared-tests": "test/",
}
INCLUDE = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]', re.M)
CODE = {".h", ".hpp", ".c", ".cpp", ".cc"}


def git(*args):
    return subprocess.check_output(
        ["git", "--no-optional-locks", "-C", str(ROOT), *args], text=True
    ).strip()


def group(path):
    return next((name for name, prefix in GROUPS.items() if path.startswith(prefix)), "other")


def inventory():
    paths = git("ls-files", "-z").split("\0")
    paths += [p.relative_to(ROOT).as_posix() for p in (ROOT / "libraries").rglob("*") if p.is_file()]
    paths = sorted(set(paths))
    paths = [p for p in paths if p and (ROOT / p).is_file()]
    tracked = set(paths)
    files = []
    edges = []
    search_roots = [ROOT / "libraries/LayerTime-Core/src", ROOT / "libraries/LayerTime-C5/src", ROOT / "sensors/src"]
    for name in ("lilygo-hunter", "lilygo-tultra", "lilygo-s3plus"):
        search_roots.append(ROOT / "devices" / name / "src")
    for path in paths:
        category = group(path)
        if category == "other":
            continue
        source = ROOT / path
        files.append({"path": path, "group": category, "bytes": source.stat().st_size,
                      "sha256": hashlib.sha256(source.read_bytes()).hexdigest()})
        if source.suffix not in CODE:
            continue
        for include in INCLUDE.findall(source.read_text(encoding="utf-8", errors="replace")):
            resolved = None
            for base in [source.parent, *search_roots]:
                candidate = (base / include).resolve()
                try:
                    relative = candidate.relative_to(ROOT).as_posix()
                except ValueError:
                    continue
                if relative in tracked or (relative.startswith("sensors/src/") and candidate.is_file()):
                    resolved = relative
                    break
            if resolved:
                edges.append({"from": path, "include": include, "to": resolved,
                              "fromGroup": category,
                              "toGroup": "sensors" if resolved.startswith("sensors/") else group(resolved)})
    counts = Counter(item["group"] for item in files)
    cross = Counter((edge["fromGroup"], edge["toGroup"]) for edge in edges
                    if edge["fromGroup"] != edge["toGroup"])
    return {
        "sourceRoot": str(ROOT), "sourceCommit": git("rev-parse", "HEAD"),
        "sourceStatus": git("status", "--porcelain=v1", "--untracked-files=normal").splitlines(),
        "sensorsRecorded": git("ls-tree", "HEAD", "sensors"),
        "groupCounts": dict(sorted(counts.items())),
        "crossGroupIncludeCounts": [{"from": a, "to": b, "count": n}
                                    for (a, b), n in sorted(cross.items())],
        "files": files, "resolvedIncludes": edges,
        "limits": ["Static include resolution is evidence of source coupling, not a linker reachability analysis.",
                   "Conditional includes are recorded regardless of build macro.",
                   "Only tracked source is inventoried; submodule source is recorded by its pinned commit.",
                   "Kotlin, Monkey C, Python, build scripts and relative resource paths need separate boundary review.",
                   "Git working-tree content is fingerprinted; this is not a claim that all files match HEAD."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = inventory()
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: report[key] for key in
                      ("sourceCommit", "sensorsRecorded", "groupCounts", "crossGroupIncludeCounts")}, indent=2))


if __name__ == "__main__":
    main()
