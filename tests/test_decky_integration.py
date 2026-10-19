"""Regressions for the Decky umask and plugin-worker permissions."""
import hashlib
import io
import os
from pathlib import Path
import tempfile
import types
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
# The helper has no .py suffix, so load source explicitly.
MODULE = types.ModuleType("ugm_decky_integration")
exec(compile((ROOT / "src/ugm-decky-integration").read_text(), "ugm-decky-integration", "exec"), MODULE.__dict__)


class DeckyIntegrationTests(unittest.TestCase):
    def test_official_installer_uses_public_umask_and_repairs_loader(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            installer = b"#!/bin/bash\nexit 0\n"
            def run(argv, **kwargs):
                self.assertEqual(kwargs["umask"], 0o022)
                self.assertEqual(kwargs["env"]["SUDO_USER"], "n3ruk")
                loader = home / "homebrew/services/PluginLoader"
                loader.parent.mkdir(parents=True)
                loader.write_bytes(b"loader")
                loader.chmod(0o700)
                return types.SimpleNamespace(returncode=0)
            with mock.patch.object(MODULE, "SCRIPT_SHA256", hashlib.sha256(installer).hexdigest()), \
                 mock.patch.object(MODULE.urllib.request, "urlopen", return_value=io.BytesIO(installer)), \
                 mock.patch.object(MODULE.shutil, "which", return_value="/usr/bin/tool"), \
                 mock.patch.object(MODULE.subprocess, "run", side_effect=run):
                self.assertTrue(MODULE.install_loader(home, "n3ruk"))
            self.assertTrue(MODULE.prepare_loader_permissions(home))
            self.assertEqual((home / "homebrew/services/PluginLoader").stat().st_mode & 0o777, 0o755)

    def test_bundled_plugin_payload_is_readable_by_worker(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            with mock.patch.object(MODULE, "PAYLOAD", ROOT / "integrations/decky"):
                for name in MODULE.NAMES:
                    self.assertTrue(MODULE.install_plugin(home, name, os.getuid(), os.getgid()))
                    target = home / "homebrew/plugins" / name
                    self.assertEqual(target.stat().st_mode & 0o777, 0o755)
                    self.assertEqual((target / "dist").stat().st_mode & 0o777, 0o755)
                    for entry in MODULE.FILES:
                        self.assertEqual((target / entry).stat().st_mode & 0o777, 0o644)


if __name__ == "__main__":
    unittest.main()
