"""Verify all shared-library build inputs against a product's content lock.

Usable as a PlatformIO pre-script or as `python tools/dependencies_check.py`.
Paths are relative to the product checkout. No download, checkout, or repair
is performed. A missing, modified, or extra library source stops the build.
"""
import hashlib
import json
from pathlib import Path
import sys


def fingerprint(path):
    data = path.read_bytes()
    if path.suffix.lower() in {".h", ".hpp", ".c", ".cpp", ".json", ".txt"}:
        data = data.replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()


def check(root):
    lock = json.loads((root / "dependencies.lock.json").read_text())
    errors = []
    for name, spec in lock["libraries"].items():
        library = (root / spec["path"]).resolve()
        want = spec["files"]
        have = {p.relative_to(library).as_posix() for p in (library / "src").rglob("*") if p.is_file()}
        have.add("library.json")
        if have != set(want):
            errors.append(f"{name}: missing {sorted(set(want)-have)}, extra {sorted(have-set(want))}")
        for relative in sorted(have & set(want)):
            p = library / relative
            if not p.is_file() or fingerprint(p) != want[relative]:
                errors.append(f"{name}: missing or modified {relative}")
    if errors:
        raise ValueError("Shared-library lock failed:\n" + "\n".join(errors))
    print("Shared-library lock verified: " + ", ".join(lock["libraries"]))


try:
    Import("env")  # noqa: F821 - PlatformIO/SCons
except NameError:
    env = None

if env is not None:
    try:
        check(Path(env.subst("$PROJECT_DIR")))
    except (OSError, ValueError, KeyError) as error:
        print(str(error))
        env.Exit(1)
elif __name__ == "__main__":
    try:
        check(Path(__file__).resolve().parents[1])
    except (OSError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
