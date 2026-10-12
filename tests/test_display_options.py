#!/usr/bin/env python3
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMAND = ROOT / "src/ugm-display-options"
SESSION = ROOT / "system/gamescope-session/steam"


class DisplayOptionsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.env = {**os.environ, "XDG_CONFIG_HOME": str(self.root / "config"),
                    "UGM_RUNTIME_DIR": str(self.root / "runtime")}
        (self.root / "runtime").mkdir()
        (self.root / "runtime/ugm-display-options").symlink_to(COMMAND)
        self.path = self.root / "config/ubuntu-gaming-mode/display-options.json"

    def command(self, *args):
        return subprocess.run([str(COMMAND), *args], env=self.env, text=True,
                              capture_output=True, check=True)

    def session(self, backend=None):
        env = self.env.copy()
        if backend:
            env["UGM_DEV_BACKEND"] = backend
        script = ('source "$1"; printf "%s %s %s %s\\n" '
                  '"$STEAM_GAMESCOPE_HDR_SUPPORTED" "$STEAM_GAMESCOPE_VRR_SUPPORTED" '
                  '"$ENABLE_GAMESCOPE_HDR" "${ADAPTIVE_SYNC:-}"')
        result = subprocess.run(["bash", "-c", script, "bash", str(SESSION)],
                                env=env, text=True, capture_output=True, check=True)
        return result.stdout.strip()

    def test_auto_preserves_proven_legacy_policy(self):
        self.assertEqual(self.session(), "1 1 1 1")
        self.assertFalse(self.path.exists())
        self.assertEqual(json.loads(self.command("--show").stdout)["auto_policy"], "legacy_2.0")

    def test_manual_off_and_on_override_auto_without_touching_inventory(self):
        inventory = self.root / "config/ubuntu-gaming-mode/inventory-v1.json"
        inventory.parent.mkdir(parents=True)
        inventory.write_text('{"hdr":"unknown"}')
        self.command("--set", "hdr", "off")
        self.assertEqual(self.session(), "0 1 0 1")
        self.command("--set", "vrr", "off")
        self.assertEqual(self.session(), "0 0 0")
        self.command("--set", "hdr", "on")
        self.assertEqual(self.session(), "1 0 1")
        self.assertEqual(json.loads(self.path.read_text())["vrr"], "off")
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)
        self.assertEqual(inventory.read_text(), '{"hdr":"unknown"}')

    def test_invalid_config_does_not_enable_features(self):
        self.path.parent.mkdir(parents=True)
        self.path.write_text('{invalid')
        self.assertEqual(self.session(), "0 0 0")
        self.assertEqual(self.command("--session").stdout, "0\n0\n")

    def test_sdl_backend_disables_both_even_when_manually_on(self):
        self.command("--set", "hdr", "on")
        self.command("--set", "vrr", "on")
        self.assertEqual(self.session("sdl"), "0 0 0")


if __name__ == "__main__":
    unittest.main()
