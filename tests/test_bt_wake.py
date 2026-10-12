"""Safety checks for the opt-in Bluetooth wake backend."""
import importlib.machinery
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


def load_backend():
    source = Path(__file__).resolve().parents[1] / "src/ugm-bt-wake"
    loader = importlib.machinery.SourceFileLoader("ugm_bt_wake", str(source))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


backend = load_backend()


class BusRollbackTests(unittest.TestCase):
    def test_restores_only_last_value_written_by_ugm(self):
        with tempfile.TemporaryDirectory() as tmp:
            wake = Path(tmp) / "wakeup"
            wake.write_text("enabled\n")
            state = {"bus": {str(wake): {
                "boot_id": "boot-a", "original": "disabled", "last": "enabled"}}}
            with patch.object(backend, "boot_id", return_value="boot-a"):
                self.assertTrue(backend.restore_bus(state))
            self.assertEqual(wake.read_text().strip(), "disabled")
            self.assertEqual(state["bus"], {})

    def test_preserves_external_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            wake = Path(tmp) / "wakeup"
            wake.write_text("disabled\n")
            state = {"bus": {str(wake): {
                "boot_id": "boot-a", "original": "disabled", "last": "enabled"}}}
            with patch.object(backend, "boot_id", return_value="boot-a"):
                self.assertFalse(backend.restore_bus(state))
            self.assertEqual(wake.read_text().strip(), "disabled")
            self.assertIn(str(wake), state["bus"])


class DeviceGuardTests(unittest.TestCase):
    def test_enable_then_reset_restores_bluez_and_usb(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            nodes = [root / f"wakeup-{n}" for n in range(3)]
            for node in nodes:
                node.write_text("disabled\n")
            item = {
                "path": "/org/bluez/hci0/dev_01_02_03_04_05_06",
                "name": "Wireless Controller", "adapter": "hci0",
                "paired": True, "trusted": True, "wake_allowed": False,
            }
            route = {"nodes": [str(node) for node in nodes]}
            def set_bluez(_bus, _path, value):
                item["wake_allowed"] = value
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend.os, "geteuid", return_value=0),
                  patch.object(backend, "boot_id", return_value="boot-a"),
                  patch.object(backend, "bluez_objects", return_value=(object(), {})),
                  patch.object(backend, "devices", return_value=[item]),
                  patch.object(backend, "wake_route", return_value=route),
                  patch.object(backend, "set_bluez", side_effect=set_bluez),
                  patch.object(backend.subprocess, "run")):
                backend.set_controller(item["path"], True)
                self.assertTrue(item["wake_allowed"])
                self.assertTrue(all(node.read_text().strip() == "enabled" for node in nodes))
                backend.reset()
                self.assertFalse(item["wake_allowed"])
                self.assertTrue(all(node.read_text().strip() == "disabled" for node in nodes))
                self.assertFalse(backend.STATE.exists())

    def test_rejects_device_path_not_in_live_paired_list(self):
        with (patch.object(backend.os, "geteuid", return_value=0),
              patch.object(backend, "bluez_objects", return_value=(object(), {})),
              patch.object(backend, "devices", return_value=[]),
              patch.object(backend, "set_bluez") as setter):
            with self.assertRaises(ValueError):
                backend.set_controller("/org/bluez/hci0/dev_00_00_00_00_00_00", True)
            setter.assert_not_called()


if __name__ == "__main__":
    unittest.main()
