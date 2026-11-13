"""Verify LayerTime's locked Git submodules before a watch build or host run.

Read-only. The lock, .gitmodules, index gitlink, checked-out commit and clean
source must agree. The index supports reviewing a dependency update before its
parent commit; after committing, Git records the same pins in HEAD. No download,
repair, checkout, or experimental bypass is performed here.
"""
import json
from pathlib import Path
import subprocess
import sys


class DependencyError(RuntimeError):
    pass


def git(root, *args):
    result = subprocess.run(
        ["git", "--no-optional-locks", "-c", "safe.directory=" + root.as_posix(),
         "-C", str(root), *args], capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise DependencyError(result.stderr.strip() or "Git dependency lookup failed")
    return result.stdout.strip()


def check(root):
    root = Path(root).resolve()
    lock = json.loads((root / "shared-repositories.lock.json").read_text(encoding="utf-8"))
    if lock.get("formatVersion") != 1 or not lock.get("repositories"):
        raise DependencyError("Unsupported or empty shared-repository lock")
    if Path(git(root, "rev-parse", "--show-toplevel")).resolve() != root:
        raise DependencyError("LayerTime must be a Git checkout")
    verified = {}
    for name, spec in lock["repositories"].items():
        relative = spec["path"]
        dependency = (root / relative).resolve()
        try:
            dependency.relative_to(root)
        except ValueError:
            raise DependencyError(name + ": dependency path leaves the LayerTime checkout")
        prefix = "submodule." + spec["submodule"]
        if git(root, "config", "--file", ".gitmodules", "--get", prefix + ".path") != relative:
            raise DependencyError(name + ": .gitmodules path differs from the lock")
        if git(root, "config", "--file", ".gitmodules", "--get", prefix + ".url") != spec["url"]:
            raise DependencyError(name + ": .gitmodules URL differs from the lock")
        entry = git(root, "ls-files", "--stage", "--", relative).split()
        if len(entry) != 4 or entry[0] != "160000" or entry[2] != "0" or entry[3] != relative:
            raise DependencyError(name + ": dependency is not one recorded Git submodule")
        if entry[1] != spec["revision"]:
            raise DependencyError(name + ": recorded gitlink differs from the lock")
        if not (dependency / ".git").exists():
            raise DependencyError(name + ": missing checkout; run git submodule update --init --recursive")
        if Path(git(dependency, "rev-parse", "--show-toplevel")).resolve() != dependency:
            raise DependencyError(name + ": dependency is not its own Git checkout")
        if git(dependency, "rev-parse", "HEAD") != spec["revision"]:
            raise DependencyError(name + ": checked-out revision differs from the lock")
        status = git(dependency, "status", "--porcelain=v1", "--untracked-files=all")
        if status:
            raise DependencyError(name + ": dependency has local changes or untracked files\n" + status)
        extra = git(dependency, "ls-files", "--others", "--ignored", "--exclude-standard", "--", *spec["buildInputs"])
        if extra:
            raise DependencyError(name + ": ignored files alter dependency build inputs\n" + extra)
        verified[name] = spec["revision"]
    return verified


def run(root):
    try:
        verified = check(root)
    except (DependencyError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print("SHARED REPOSITORY CHECK FAILED: " + str(error))
        return 1
    print("SHARED REPOSITORY CHECK PASSED: " + ", ".join(name + "@" + sha[:12] for name, sha in verified.items()))
    return 0


try:
    Import("env")  # noqa: F821 - PlatformIO/SCons
except NameError:
    env = None

if env is not None:
    project_root = Path(env.subst("$PROJECT_DIR")).resolve().parents[1]
    result = run(project_root)
    if result:
        env.Exit(result)
elif __name__ == "__main__":
    sys.exit(run(Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parents[1]))
