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
    def setUp(self):
        self.marker_dir = tempfile.TemporaryDirectory()
        root = Path(self.marker_dir.name)
        self.marker_patches = (
            patch.object(backend, "USB_POLICY_MARKER", root / "usb-policy"),
            patch.object(backend, "BT_ENABLED_MARKER", root / "bt-enabled"),
        )
        for item in self.marker_patches:
            item.start()
            self.addCleanup(item.stop)
        self.addCleanup(self.marker_dir.cleanup)

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
                  patch.object(backend, "usb_routes", return_value=[route]),
                  patch.object(backend, "set_bluez", side_effect=set_bluez)):
                backend.prepare(seed_existing=True)
                self.assertTrue(all(node.read_text().strip() == "enabled" for node in nodes))
                self.assertFalse(item["wake_allowed"])
                backend.set_controller(item["path"], True)
                self.assertTrue(item["wake_allowed"])
                self.assertTrue(all(node.read_text().strip() == "enabled" for node in nodes))
                backend.reset()
                self.assertFalse(item["wake_allowed"])
                self.assertTrue(all(node.read_text().strip() == "disabled" for node in nodes))
                self.assertFalse(backend.STATE.exists())


    def test_setting_controller_does_not_change_usb_or_services(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            item = {"path": "/org/bluez/hci0/dev_01_02_03_04_05_06",
                    "name": "Wireless Controller", "adapter": "hci0",
                    "paired": True, "trusted": True, "wake_allowed": False}
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend.os, "geteuid", return_value=0),
                  patch.object(backend, "bluez_objects", return_value=(object(), {})),
                  patch.object(backend, "devices", return_value=[item]),
                  patch.object(backend, "enable_bus") as enable_bus,
                  patch.object(backend, "set_bluez") as set_bluez):
                backend.set_controller(item["path"], True)
                set_bluez.assert_called_once_with(unittest.mock.ANY, item["path"], True)
                enable_bus.assert_not_called()

    def test_new_pairing_defaults_off_but_existing_choice_is_untouched(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            old = {"path": "/org/bluez/hci0/dev_01_02_03_04_05_06", "wake_allowed": True}
            new = {"path": "/org/bluez/hci0/dev_06_05_04_03_02_01", "wake_allowed": True}
            def set_wake(_bus, path, value):
                if path == new["path"]:
                    new["wake_allowed"] = value
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend, "devices", return_value=[old, new]),
                  patch.object(backend, "set_bluez", side_effect=set_wake) as set_bluez):
                backend.save_state({"version": 1, "controllers": {}, "bus": {}, "known": [old["path"]]})
                backend.disable_new_controllers(object(), {})
                self.assertEqual(set_bluez.call_count, 1)
                self.assertEqual(set_bluez.call_args.args[1:], (new["path"], False))
                state = backend.load_state()
                self.assertEqual(state["known"], [old["path"], new["path"]])
                self.assertEqual(state["controllers"][new["path"]]["original"], True)
                backend.disable_new_controllers(object(), {})
                self.assertEqual(set_bluez.call_count, 1)

    def test_watcher_reapplies_saved_choice_after_bluez_changes_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            no = {"path": "/org/bluez/hci0/dev_01_02_03_04_05_06", "wake_allowed": True}
            yes = {"path": "/org/bluez/hci0/dev_06_05_04_03_02_01", "wake_allowed": False}
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend, "devices", return_value=[no, yes]),
                  patch.object(backend, "set_bluez") as set_bluez):
                backend.save_state({"version": 1, "controllers": {
                    no["path"]: {"original": True, "desired": False, "last": False},
                    yes["path"]: {"original": False, "desired": True, "last": True}},
                    "bus": {}, "known": [no["path"], yes["path"]]})
                backend.disable_new_controllers(object(), {})
                self.assertEqual({call.args[1:] for call in set_bluez.call_args_list},
                                 {(no["path"], False), (yes["path"], True)})

    def test_uninstall_restores_new_controller_default(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            item = {"path": "/org/bluez/hci0/dev_01_02_03_04_05_06",
                    "wake_allowed": True}
            def set_bluez(_bus, _path, value):
                item["wake_allowed"] = value
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend.os, "geteuid", return_value=0),
                  patch.object(backend, "devices", return_value=[item]),
                  patch.object(backend, "bluez_objects", return_value=(object(), {})),
                  patch.object(backend, "set_bluez", side_effect=set_bluez)):
                backend.disable_new_controllers(object(), {})
                self.assertFalse(item["wake_allowed"])
                backend.reset()
                self.assertTrue(item["wake_allowed"])
                self.assertFalse(backend.STATE.exists())

    def test_new_controller_already_off_is_persisted_for_next_boot(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            item = {"path": "/org/bluez/hci0/dev_01_02_03_04_05_06",
                    "wake_allowed": False}
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend, "devices", return_value=[item]),
                  patch.object(backend, "set_bluez") as set_bluez):
                backend.save_state({"version": 1, "controllers": {}, "bus": {}, "known": []})
                backend.disable_new_controllers(object(), {})
                set_bluez.assert_not_called()
                self.assertEqual(backend.load_state()["controllers"][item["path"]],
                                 {"original": False, "desired": False, "last": False})

    def test_boot_reapplies_saved_yes_and_no(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            yes = {"path": "/org/bluez/hci0/dev_01_02_03_04_05_06", "wake_allowed": False}
            no = {"path": "/org/bluez/hci0/dev_06_05_04_03_02_01", "wake_allowed": True}
            state = {"version": 1, "bus": {}, "known": [yes["path"], no["path"]],
                     "controllers": {
                         yes["path"]: {"original": False, "desired": True, "last": True},
                         no["path"]: {"original": True, "desired": False, "last": False}}}
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend.os, "geteuid", return_value=0),
                  patch.object(backend, "prepare") as prepare,
                  patch.object(backend, "bluez_objects", return_value=(object(), {})),
                  patch.object(backend, "devices", return_value=[yes, no]),
                  patch.object(backend, "set_bluez") as set_bluez):
                backend.save_state(state)
                backend.apply()
                prepare.assert_called_once_with(seed_existing=True)
                self.assertEqual({call.args[1:] for call in set_bluez.call_args_list},
                                 {(yes["path"], True), (no["path"], False)})

    def test_rejects_device_path_not_in_live_paired_list(self):
        with (patch.object(backend.os, "geteuid", return_value=0),
              patch.object(backend, "bluez_objects", return_value=(object(), {})),
              patch.object(backend, "devices", return_value=[]),
              patch.object(backend, "set_bluez") as setter):
            with self.assertRaises(ValueError):
                backend.set_controller("/org/bluez/hci0/dev_00_00_00_00_00_00", True)
            setter.assert_not_called()


class UsbPolicyTests(unittest.TestCase):
    def test_fresh_install_denies_existing_and_hotplug_usb_but_not_bluetooth(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            usb_root = root / "usb"
            usb_root.mkdir()

            def device(name):
                node = usb_root / name
                (node / "power").mkdir(parents=True)
                (node / "idVendor").write_text("1234")
                (node / "idProduct").write_text("5678")
                (node / "devnum").write_text("3")
                wake = node / "power/wakeup"
                wake.write_text("enabled\n")
                return wake

            bt = device("1-14")
            dell = device("1-8")
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend, "USB_POLICY_MARKER", root / "policy"),
                  patch.object(backend, "BT_ENABLED_MARKER", root / "bt-enabled"),
                  patch.object(backend, "USB_DEVICES", usb_root),
                  patch.object(backend, "bluetooth_usb_nodes", return_value={str(bt.resolve())}),
                  patch.object(backend, "boot_id", return_value="boot-a"),
                  patch.object(backend.os, "geteuid", return_value=0),
                  patch.object(backend, "bluez_objects", return_value=(object(), {})),
                  patch.object(backend, "devices", return_value=[])):
                backend.init_fresh_install()
                self.assertTrue(backend.load_state()["strict_controllers"])
                backend.deny_usb()
                self.assertEqual(bt.read_text().strip(), "enabled")
                self.assertEqual(dell.read_text().strip(), "disabled")
                pad = device("1-9")
                backend.deny_usb("1-9")
                self.assertEqual(pad.read_text().strip(), "disabled")
                backend.deny_usb("../1-14")
                self.assertEqual(bt.read_text().strip(), "enabled")
                backend.reset()
                self.assertEqual(dell.read_text().strip(), "enabled")
                self.assertEqual(pad.read_text().strip(), "enabled")
                self.assertFalse(backend.STATE.exists())
                self.assertFalse(backend.USB_POLICY_MARKER.exists())

    def test_existing_25_state_is_not_migrated(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend, "USB_POLICY_MARKER", root / "policy"),
                  patch.object(backend.os, "geteuid", return_value=0)):
                backend.save_state({"version": 1, "controllers": {}, "bus": {}, "known": []})
                backend.init_fresh_install()
                state = backend.load_state()
                self.assertNotIn("usb_policy", state)
                self.assertFalse(backend.USB_POLICY_MARKER.exists())

    def test_fresh_install_disables_already_paired_controller(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            usb_root = root / "usb"
            usb_root.mkdir()
            nodes = [root / f"route-{n}" for n in range(3)]
            for node in nodes:
                node.write_text("disabled\n")
            item = {"path": "/org/bluez/hci0/dev_01_02_03_04_05_06",
                    "wake_allowed": True, "trusted": True}
            def set_bluez(_bus, _path, value):
                item["wake_allowed"] = value
            with (patch.object(backend, "STATE", root / "bt-wake.json"),
                  patch.object(backend, "USB_POLICY_MARKER", root / "policy"),
                  patch.object(backend, "USB_DEVICES", usb_root),
                  patch.object(backend, "boot_id", return_value="boot-a"),
                  patch.object(backend.os, "geteuid", return_value=0),
                  patch.object(backend, "usb_routes", return_value=[{"nodes": [str(n) for n in nodes]}]),
                  patch.object(backend, "bluez_objects", return_value=(object(), {})),
                  patch.object(backend, "devices", return_value=[item]),
                  patch.object(backend, "set_bluez", side_effect=set_bluez)):
                backend.init_fresh_install()
                backend.apply()
                self.assertFalse(item["wake_allowed"])
                self.assertFalse(backend.load_state()["controllers"][item["path"]]["desired"])
                self.assertTrue(all(n.read_text().strip() == "enabled" for n in nodes))


if __name__ == "__main__":
    unittest.main()
