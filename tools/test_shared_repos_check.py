import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from shared_repos_check import DependencyError, check


class SharedRepositoryChecks(unittest.TestCase):
    def git(self, folder, *args):
        return subprocess.check_output(
            ["git", "-c", "user.name=Test", "-c", "user.email=test@example.invalid",
             "-C", str(folder), *args], text=True, stderr=subprocess.PIPE).strip()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="layertime-dependency-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.child = self.root / "deps/shared"
        self.child.mkdir(parents=True)
        for folder in (self.root, self.child):
            self.git(folder, "init", "-q", "-b", "main")
            self.git(folder, "config", "core.autocrlf", "false")
        (self.child / "src").mkdir()
        (self.child / "src/API.h").write_text("void function();\n", encoding="utf-8")
        (self.child / "library.json").write_text('{"name":"Example"}\n', encoding="utf-8")
        (self.child / ".gitignore").write_text("src/ignored.cpp\n", encoding="utf-8")
        self.git(self.child, "add", "--", "src/API.h", "library.json", ".gitignore")
        self.git(self.child, "commit", "-q", "-m", "fixture")
        self.sha = self.git(self.child, "rev-parse", "HEAD")
        self.git(self.root, "update-index", "--add", "--cacheinfo", "160000," + self.sha + ",deps/shared")
        (self.root / ".gitmodules").write_text(
            '[submodule "example"]\n\tpath = deps/shared\n\turl = https://example.invalid/shared.git\n', encoding="utf-8")
        self.lock = {"formatVersion": 1, "repositories": {"Example": {
            "submodule": "example", "path": "deps/shared", "url": "https://example.invalid/shared.git",
            "revision": self.sha, "buildInputs": ["src", "library.json"]}}}
        self.save_lock()

    def save_lock(self):
        (self.root / "shared-repositories.lock.json").write_text(json.dumps(self.lock), encoding="utf-8")

    def test_recorded_clean_checkout_passes(self):
        self.assertEqual({"Example": self.sha}, check(self.root))

    def test_tracked_source_edit_fails(self):
        (self.child / "src/API.h").write_text("void changed();\n", encoding="utf-8")
        with self.assertRaisesRegex(DependencyError, "local changes"):
            check(self.root)

    def test_ignored_extra_build_input_fails(self):
        (self.child / "src/ignored.cpp").write_text("void unexpected() {}\n", encoding="utf-8")
        with self.assertRaisesRegex(DependencyError, "ignored files"):
            check(self.root)

    def test_unrecorded_dependency_commit_fails(self):
        (self.child / "src/API.h").write_text("void changed();\n", encoding="utf-8")
        self.git(self.child, "add", "--", "src/API.h")
        self.git(self.child, "commit", "-q", "-m", "new source")
        with self.assertRaisesRegex(DependencyError, "checked-out revision"):
            check(self.root)

    def test_lock_update_without_gitlink_update_fails(self):
        self.lock["repositories"]["Example"]["revision"] = "0" * 40
        self.save_lock()
        with self.assertRaisesRegex(DependencyError, "gitlink differs"):
            check(self.root)

    def test_missing_dependency_fails_with_fetch_instruction(self):
        destination = self.root / "unavailable"
        self.child.resolve().relative_to(self.root)
        destination.resolve().relative_to(self.root)
        self.child.rename(destination)
        with self.assertRaisesRegex(DependencyError, "submodule update --init"):
            check(self.root)

    def test_unexpected_repository_url_fails(self):
        self.lock["repositories"]["Example"]["url"] = "https://example.invalid/other.git"
        self.save_lock()
        with self.assertRaisesRegex(DependencyError, "URL differs"):
            check(self.root)


if __name__ == "__main__":
    unittest.main()
