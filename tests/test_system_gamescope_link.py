#!/usr/bin/env python3
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "src" / "ugm-system-gamescope-link"


class SystemGamescopeLinkTests(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="ugm-gamescope-link-"))
        self.target = self.tmp / "runtime" / "gamescope"
        self.link = self.tmp / "local" / "bin" / "gamescope"
        self.state = self.tmp / "state"
        self.original = self.state / "original-state"
        self.marker = self.state / "system-gamescope-link.managed"

        self.target.parent.mkdir(parents=True)
        self.target.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        self.target.chmod(0o755)

        (self.original / "files").mkdir(parents=True)
        (self.original / "format-version").write_text("1\n", encoding="utf-8")
        (self.original / ".complete").touch()

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def env(self):
        result = os.environ.copy()
        result.update(
            UGM_GAMESCOPE_TARGET=str(self.target),
            UGM_GAMESCOPE_LINK=str(self.link),
            UGM_STATE_ROOT=str(self.state),
            UGM_ORIGINAL_STATE=str(self.original),
            UGM_GAMESCOPE_MARKER=str(self.marker),
        )
        return result

    def finalize_snapshot(self, status, original_content=None):
        manifest = f"{status}\t{self.link}\n"
        (self.original / "manifest").write_text(manifest, encoding="utf-8")
        digest = hashlib.sha256(manifest.encode()).hexdigest()
        (self.original / "manifest.sha256").write_text(
            f"{digest}  manifest\n", encoding="utf-8"
        )

        if original_content is not None:
            stored = self.original / "files" / self.link.relative_to("/")
            stored.parent.mkdir(parents=True, exist_ok=True)
            stored.write_text(original_content, encoding="utf-8")
            stored.chmod(0o755)

    def run_helper(self, action, check=True):
        return subprocess.run(
            [str(HELPER), action],
            env=self.env(),
            text=True,
            capture_output=True,
            check=check,
        )

    def test_enable_and_disable_when_original_was_absent(self):
        self.finalize_snapshot("absent")
        self.run_helper("--enable")
        self.assertTrue(self.link.is_symlink())
        self.assertEqual(os.readlink(self.link), str(self.target))
        self.assertTrue(self.marker.exists())
        self.assertEqual(self.run_helper("--check").stdout.strip(), "managed")

        self.run_helper("--disable")
        self.assertFalse(self.link.exists())
        self.assertFalse(self.marker.exists())

    def test_disable_restores_original_file(self):
        self.finalize_snapshot("present", "original-gamescope\n")
        self.run_helper("--enable")
        self.run_helper("--disable")
        self.assertFalse(self.link.is_symlink())
        self.assertEqual(self.link.read_text(encoding="utf-8"), "original-gamescope\n")

    def test_disable_restores_original_symlink(self):
        self.finalize_snapshot("present")
        stored = self.original / "files" / self.link.relative_to("/")
        stored.parent.mkdir(parents=True, exist_ok=True)
        stored.symlink_to("/opt/original-gamescope")
        self.link.parent.mkdir(parents=True, exist_ok=True)
        self.link.symlink_to("/opt/original-gamescope")

        self.run_helper("--enable")
        self.run_helper("--disable")
        self.assertTrue(self.link.is_symlink())
        self.assertEqual(os.readlink(self.link), "/opt/original-gamescope")

    def test_disable_preserves_external_change(self):
        self.finalize_snapshot("absent")
        self.run_helper("--enable")
        self.link.unlink()
        self.link.write_text("external\n", encoding="utf-8")
        self.run_helper("--disable")
        self.assertEqual(self.link.read_text(encoding="utf-8"), "external\n")
        self.assertFalse(self.marker.exists())

    def test_enable_refuses_directory(self):
        self.finalize_snapshot("absent")
        self.link.mkdir(parents=True)
        result = self.run_helper("--enable", check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(self.link.is_dir())

    def test_enable_refuses_invalid_snapshot(self):
        self.finalize_snapshot("absent")
        (self.original / "manifest.sha256").write_text(
            "0" * 64 + "  manifest\n", encoding="utf-8"
        )
        result = self.run_helper("--enable", check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.link.exists())

    def test_disable_without_marker_changes_nothing(self):
        self.finalize_snapshot("absent")
        self.link.parent.mkdir(parents=True)
        self.link.write_text("external\n", encoding="utf-8")
        self.run_helper("--disable")
        self.assertEqual(self.link.read_text(encoding="utf-8"), "external\n")

    def test_debian_package_wiring_is_opt_in_and_reversible(self):
        templates = (ROOT / "packaging" / "DEBIAN" / "templates").read_text()
        config = (ROOT / "packaging" / "DEBIAN" / "config").read_text()
        preinst = (ROOT / "packaging" / "DEBIAN" / "preinst").read_text()
        postinst = (ROOT / "packaging" / "DEBIAN" / "postinst").read_text()
        build = (ROOT / "packaging" / "build-deb.sh").read_text()
        doctor = (ROOT / "src" / "ubuntu-gaming-mode-doctor").read_text()

        stanza = templates.split(
            "Template: ubuntu-gaming-mode/system-gamescope-command", 1
        )[1]
        self.assertIn("Default: false", stanza)
        self.assertIn(
            "db_input high ubuntu-gaming-mode/system-gamescope-command", config
        )
        self.assertIn("/usr/local/bin/gamescope", preinst)
        self.assertIn(
            "db_get ubuntu-gaming-mode/system-gamescope-command", postinst
        )
        self.assertIn("ugm-system-gamescope-link --enable", postinst)
        self.assertIn("ugm-system-gamescope-link --disable", postinst)
        self.assertIn("src/ugm-system-gamescope-link", build)
        self.assertIn("ugm-system-gamescope-link", doctor)
        self.assertNotRegex(HELPER.read_text(), r"\b(?:apt|apt-get|dpkg)\b")


if __name__ == "__main__":
    unittest.main()
