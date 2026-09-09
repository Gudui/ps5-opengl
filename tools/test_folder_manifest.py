#!/usr/bin/env python3
"""Regression tests for native title-folder manifest generation."""

import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("write_folder_manifest", ROOT / "write-folder-manifest.py")
WRITER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRITER)


class FolderManifestTests(unittest.TestCase):
    def populate(self, root: Path) -> Path:
        title = root / "PPSA99202"
        (title / "sce_sys").mkdir(parents=True)
        (title / "eboot.bin").write_bytes(b"new eboot")
        (title / "sce_sys" / "param.json").write_text('{"titleId":"PPSA99202"}\n')
        return title

    def test_replaces_stale_manifest_with_complete_sorted_receipt(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.populate(root)
            manifest = root / "folder-manifest.sha256"
            manifest.write_text("old-hash  PPSA99202/eboot.bin\n")

            WRITER.write_manifest(root, "PPSA99202")

            lines = manifest.read_text().splitlines()
            self.assertEqual(2, len(lines))
            self.assertEqual(sorted(lines), lines)
            self.assertTrue(all(line.startswith("PPSA99202/") is False for line in lines))
            self.assertTrue(any(line.endswith("  PPSA99202/eboot.bin") for line in lines))
            self.assertTrue(any(line.endswith("  PPSA99202/sce_sys/param.json") for line in lines))

    def test_rejects_symlink_payload(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            title = self.populate(root)
            target = root / "outside.bin"
            target.write_bytes(b"outside")
            link = title / "linked.bin"
            try:
                link.symlink_to(target)
            except OSError as error:
                self.skipTest(f"symlink unavailable: {error}")

            with self.assertRaisesRegex(ValueError, "symlink"):
                WRITER.write_manifest(root, "PPSA99202")


if __name__ == "__main__":
    unittest.main()
