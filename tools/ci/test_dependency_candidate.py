"""Stage one shared-library candidate in an ephemeral CI consumer checkout.

Never edits production pins remotely. The existing dependency guards still run
against a consistent index and lock. Candidate artifacts are labelled separately.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def main():
    if os.environ.get("GITHUB_ACTIONS") != "true":
        raise SystemExit("Candidate staging is only allowed in ephemeral GitHub Actions checkouts")
    root = Path(os.environ["PRODUCT_ROOT"]).resolve()
    name = os.environ["CANDIDATE_REPOSITORY"]
    revision = os.environ["CANDIDATE_REVISION"]
    paths = {"LayerTime-Core": "libraries/LayerTime-Core", "LayerTime-C5": "libraries/LayerTime-C5",
             "LayerTime-Sensors": "sensors", "LayerTime-Contracts": "contracts"}
    if name not in paths or not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise SystemExit("Provide an allowed library name and a full 40-character commit SHA")
    relative = paths[name]
    child = root / relative
    if git(root, "status", "--porcelain") or git(child, "status", "--porcelain"):
        raise SystemExit("Candidate checkout must start clean")
    entry = git(root, "ls-files", "--stage", "--", relative).split()
    if len(entry) != 4 or entry[0] != "160000":
        raise SystemExit("This product does not consume the requested library")
    consumer = git(root, "rev-parse", "HEAD")
    before = git(child, "rev-parse", "HEAD")
    url = "https://github.com/mikevan/" + name + ".git"
    git(child, "fetch", "--depth=1", url, revision)
    git(child, "checkout", "--detach", revision)
    git(root, "add", "--", relative)
    for filename, key in (("shared-repositories.lock.json", "repositories"),
                          ("dependencies.lock.json", "libraries")):
        path = root / filename
        if not path.exists():
            continue
        lock = json.loads(path.read_text(encoding="utf-8"))
        spec = lock[key][name]
        if spec["path"] != relative or spec["url"] != url:
            raise SystemExit("Candidate does not match the canonical dependency lock")
        spec["revision"] = revision
        if "files" in spec:
            # Use the same tracked build-input boundary and newline normalization
            # as Hunter's verifier; include new candidate sources, exclude tools.
            names = git(child, "ls-files", "--", "src", "library.json").splitlines()
            spec["files"] = {}
            for item in names:
                source = child / item
                data = source.read_bytes()
                if source.suffix.lower() in {".h", ".hpp", ".c", ".cpp", ".json", ".txt"}:
                    data = data.replace(b"\r\n", b"\n")
                spec["files"][item] = hashlib.sha256(data).hexdigest()
        path.write_text(json.dumps(lock, indent=2) + "\n", encoding="utf-8")
        git(root, "add", "--", filename)
    # The legacy Sensors guard reads HEAD, whereas newer guards read the index.
    # Record a local-only integration commit so both inspect the same candidate.
    if git(root, "diff", "--cached", "--name-only"):
        git(root, "-c", "user.name=Dependency CI", "-c", "user.email=ci@example.invalid",
            "-c", "commit.gpgsign=false", "commit", "-m", "Test shared-library candidate locally")
    record = {"consumer": consumer, "library": name,
              "previousRevision": before, "candidateRevision": revision,
              "productionPinsChanged": False}
    (root / "candidate-validation.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record))


if __name__ == "__main__":
    main()
