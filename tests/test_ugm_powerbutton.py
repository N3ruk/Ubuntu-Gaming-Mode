#!/usr/bin/python3
from __future__ import annotations

import importlib.machinery
import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/ugm-powerbuttond"
loader = importlib.machinery.SourceFileLoader("ugm_powerbuttond", str(SOURCE))
spec = importlib.util.spec_from_loader(loader.name, loader)
module = importlib.util.module_from_spec(spec)
sys.modules[loader.name] = module
loader.exec_module(module)

HELPER_SOURCE = ROOT / "src/ugm-steam-shortpowerpress"
helper_loader = importlib.machinery.SourceFileLoader("ugm_steam_shortpowerpress", str(HELPER_SOURCE))
helper_spec = importlib.util.spec_from_loader(helper_loader.name, helper_loader)
helper = importlib.util.module_from_spec(helper_spec)
sys.modules[helper_loader.name] = helper
helper_loader.exec_module(helper)


class PowerButtonTests(unittest.TestCase):
    def test_short_press_only(self):
        device = module.InputDevice(Path("/dev/null"), None)
        self.assertFalse(module.handle_event(device, (0, 0, 1, 116, 1), 1.0))
        self.assertTrue(module.handle_event(device, (0, 0, 1, 116, 0), 1.4))

    def test_long_press_is_not_dispatched(self):
        device = module.InputDevice(Path("/dev/null"), None)
        self.assertFalse(module.handle_event(device, (0, 0, 1, 116, 1), 1.0))
        self.assertFalse(module.handle_event(device, (0, 0, 1, 116, 0), 3.0))

    def test_unrelated_key_is_ignored(self):
        device = module.InputDevice(Path("/dev/null"), None)
        self.assertFalse(module.handle_event(device, (0, 0, 1, 30, 1), 1.0))

    def test_marker_parser_is_closed_and_permission_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / "marker"
            marker.write_text("version=1\nuid=1000\ninstance=steam\npid=123\n")
            marker.chmod(0o600)
            self.assertEqual(module.parse_marker(marker)["instance"], "steam")
            marker.chmod(0o644)
            self.assertIsNone(module.parse_marker(marker))
            marker.chmod(0o600)
            marker.write_text("version=1\nuid=1000\ninstance=steam\npid=123\nextra=x\n")
            self.assertIsNone(module.parse_marker(marker))

    def test_marker_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "target"
            marker = Path(directory) / "marker"
            target.write_text("version=1\nuid=1000\ninstance=steam\npid=123\n")
            target.chmod(0o600)
            marker.symlink_to(target)
            self.assertIsNone(module.parse_marker(marker))

    @mock.patch.object(module, "steam_environment", return_value={"HOME": "/home/test"})
    @mock.patch.object(module, "steam_process", return_value=999)
    @mock.patch.object(module.subprocess, "run")
    def test_native_dispatch_hands_off_to_fixed_user_unit(
        self, run, _steam_process, _steam_environment
    ):
        run.return_value = subprocess.CompletedProcess([], 0, "", "")
        session = module.GamingSession(1000, "steam", 123, Path("/home/test"), Path("/run/user/1000"))
        self.assertTrue(module.trigger_native_suspend(session))
        args, kwargs = run.call_args
        self.assertEqual(
            args[0],
            ["/usr/bin/systemctl", "--user", "start", "ugm-steam-shortpowerpress.service"],
        )
        self.assertNotIn("shell", kwargs)
        self.assertTrue(callable(kwargs["preexec_fn"]))

    @mock.patch.object(module, "steam_process", return_value=None)
    @mock.patch.object(module.subprocess, "run")
    def test_missing_gamepadui_never_dispatches(self, run, _steam_process):
        session = module.GamingSession(1000, "steam", 123, Path("/home/test"), Path("/run/user/1000"))
        self.assertFalse(module.trigger_native_suspend(session))
        run.assert_not_called()

    @mock.patch.object(helper, "marker_is_valid", return_value=True)
    @mock.patch.object(helper, "steam_process", return_value=999)
    @mock.patch.object(helper, "steam_binary", return_value=Path("/usr/bin/steam"))
    @mock.patch.object(helper.subprocess, "run")
    @mock.patch.object(helper.os, "getuid", return_value=1000)
    @mock.patch.dict(helper.os.environ, {"XDG_RUNTIME_DIR": "/run/user/1000"}, clear=False)
    @mock.patch.object(helper.Path, "home", return_value=Path("/home/test"))
    def test_user_helper_uses_only_fixed_steam_uri(
        self, _home, _uid, run, _binary, _steam_process, _marker
    ):
        run.return_value = subprocess.CompletedProcess([], 0, "", "")
        self.assertEqual(helper.run(), 0)
        args, kwargs = run.call_args
        self.assertEqual(args[0], ["/usr/bin/steam", "-ifrunning", "steam://shortpowerpress"])
        self.assertNotIn("shell", kwargs)

    @mock.patch.object(helper, "marker_is_valid", return_value=False)
    @mock.patch.object(helper.subprocess, "run")
    @mock.patch.object(helper.os, "getuid", return_value=1000)
    @mock.patch.dict(helper.os.environ, {"XDG_RUNTIME_DIR": "/run/user/1000"}, clear=False)
    @mock.patch.object(helper.Path, "home", return_value=Path("/home/test"))
    def test_user_helper_never_dispatches_without_valid_gaming_marker(
        self, _home, _uid, run, _marker
    ):
        self.assertEqual(helper.run(), 1)
        run.assert_not_called()

    def test_user_helper_self_test(self):
        self.assertEqual(helper.self_test(), 0)

    def test_package_default_is_opt_in(self):
        templates = (ROOT / "packaging/DEBIAN/templates").read_text()
        self.assertIn("Default: false", templates)
        service = (ROOT / "system/systemd/ugm-powerbuttond.service").read_text()
        self.assertIn("WantedBy=multi-user.target", service)
        self.assertFalse((ROOT / "system/systemd/multi-user.target.wants").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
