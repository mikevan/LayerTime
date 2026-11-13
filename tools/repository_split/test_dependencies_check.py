import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from dependencies_check import check, fingerprint


class DependencyLockTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "library/src").mkdir(parents=True)
        self.header = self.root / "library/src/API.h"
        self.header.write_bytes(b"// public API\nvoid function();\n")
        manifest = self.root / "library/library.json"
        manifest.write_text('{"name":"test-library"}', encoding="utf-8")
        lock = {"libraries": {"test-library": {"path": "library", "files": {
            "library.json": fingerprint(manifest), "src/API.h": fingerprint(self.header)}}}}
        (self.root / "dependencies.lock.json").write_text(json.dumps(lock), encoding="utf-8")

    def verify(self):
        with contextlib.redirect_stdout(io.StringIO()):
            check(self.root)

    def test_identical_sources_and_windows_line_endings_are_accepted(self):
        self.verify()
        self.header.write_bytes(self.header.read_bytes().replace(b"\n", b"\r\n"))
        self.verify()

    def test_changed_source_is_rejected(self):
        self.header.write_text("void changed();", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "modified"):
            self.verify()

    def test_missing_source_is_rejected(self):
        self.header.unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            self.verify()

    def test_extra_source_is_rejected(self):
        (self.header.parent / "unexpected.cpp").write_text("void surprise() {}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "extra"):
            self.verify()


if __name__ == "__main__":
    unittest.main()
