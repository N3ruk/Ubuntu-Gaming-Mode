#!/usr/bin/env python3
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "prototypes/ugm-vrs-run"
STEAM_CONFIG = ROOT / "system/gamescope-session/steam"


class VrsRunTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.inventory = Path(self.tmp.name) / "inventory.json"
        self.data = {
            "relation": {"state": "matched"},
            "vulkan": {"selected": {"deviceUUID": "amd-1", "driverID": "DRIVER_ID_MESA_RADV",
                                    "gpu_pci_id": "1002:73bf"},
                       "devices": [{"deviceUUID": "amd-1", "software": False}]},
            "capabilities": {"radv_vrs": {"state": "present_unvalidated"}},
        }
        self.env = os.environ.copy()
        layer_dir = Path(self.tmp.name) / "layer"
        layer_dir.mkdir()
        for name in ("libugm_nvidia_vrs_64.so", "libugm_nvidia_vrs_32.so",
                     "VK_LAYER_UGM_nvidia_vrs_64.json", "VK_LAYER_UGM_nvidia_vrs_32.json"):
            (layer_dir / name).touch()
        self.env["UGM_VRS_LAYER_DIR"] = str(layer_dir)
        for name in ("DRI_PRIME", "MESA_VK_DEVICE_SELECT", "VK_ICD_FILENAMES", "VK_DRIVER_FILES",
                     "DXVK_FILTER_DEVICE_NAME", "VKD3D_FILTER_DEVICE_NAME", "__NV_PRIME_RENDER_OFFLOAD"):
            self.env.pop(name, None)

    def run_tool(self, *args):
        self.inventory.write_text(json.dumps(self.data))
        return subprocess.run([str(RUNNER), "--inventory-json", str(self.inventory), *args],
                              env=self.env, text=True, capture_output=True)

    def test_amd_only_child_receives_rate_without_file_override(self):
        self.env["RADV_FORCE_VRS_CONFIG_FILE"] = "/tmp/old-vrs-config"
        child = "import os; print(os.getenv('RADV_FORCE_VRS'), os.getenv('RADV_FORCE_VRS_CONFIG_FILE'))"
        result = self.run_tool("--rate", "2x2", "--confirm-gfx103", "--", sys.executable, "-c", child)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "2x2 None")
        self.assertNotIn("RADV_FORCE_VRS", os.environ)

    def test_nvidia_never_receives_amd_override(self):
        self.data["vulkan"]["selected"]["driverID"] = "DRIVER_ID_NVIDIA_PROPRIETARY"
        self.data["vulkan"]["selected"]["gpu_pci_id"] = "10de:1f08"
        self.assertEqual(self.run_tool("--confirm-gfx103", "--", "true").returncode, 2)

    def test_nvidia_child_gets_layer_without_radv_variables(self):
        self.data["vulkan"]["selected"].update(
            driverID="DRIVER_ID_NVIDIA_PROPRIETARY", gpu_pci_id="10de:1f08")
        self.data["capabilities"] = {"nvidia_layer_vrs": {"state": "present_unvalidated"}}
        self.env["RADV_FORCE_VRS"] = "2x2"
        self.env["RADV_FORCE_VRS_CONFIG_FILE"] = "/tmp/old-vrs-config"
        child = ("import os; print(os.getenv('RADV_FORCE_VRS'), "
                 "os.getenv('RADV_FORCE_VRS_CONFIG_FILE'), "
                 "os.getenv('UGM_VRS_RATE'), os.getenv('VK_INSTANCE_LAYERS'))")
        result = self.run_tool("--rate", "2x2", "--", sys.executable, "-c", child)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "None None 2x2 VK_LAYER_UGM_nvidia_vrs")

    def test_steam_toggle_routes_file_to_nvidia_not_radv(self):
        self.data["vulkan"]["selected"].update(
            driverID="DRIVER_ID_NVIDIA_PROPRIETARY", gpu_pci_id="10de:1f08")
        self.data["capabilities"] = {"nvidia_layer_vrs": {"state": "present_unvalidated"}}
        rate_file = Path(self.tmp.name) / "rate"
        rate_file.write_text("2x2\n")
        self.env["RADV_FORCE_VRS_CONFIG_FILE"] = str(rate_file)
        child = ("import os; print(os.getenv('RADV_FORCE_VRS_CONFIG_FILE'), "
                 "os.getenv('UGM_VRS_CONFIG_FILE'))")
        result = self.run_tool("--steam-toggle", "--", sys.executable, "-c", child)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), f"None {rate_file}")

    def test_steam_toggle_keeps_amd_file_only_on_integrated_gpu(self):
        rate_file = Path(self.tmp.name) / "rate"
        rate_file.write_text("2x2\n")
        self.env["RADV_FORCE_VRS_CONFIG_FILE"] = str(rate_file)
        child = "import os; print(os.getenv('RADV_FORCE_VRS_CONFIG_FILE'), os.getenv('RADV_FORCE_VRS'))"
        self.data["vulkan"]["selected"]["deviceType"] = "PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU"
        result = self.run_tool("--steam-toggle", "--confirm-gfx103", "--", sys.executable, "-c", child)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), f"{rate_file} None")
        self.data["vulkan"]["selected"]["deviceType"] = "PHYSICAL_DEVICE_TYPE_DISCRETE_GPU"
        self.assertEqual(self.run_tool("--steam-toggle", "--confirm-gfx103", "--", "true").returncode, 2)

    def test_uncertain_gpu_or_redirector_fails_closed(self):
        self.data["vulkan"]["devices"].append({"deviceUUID": "other", "software": False})
        self.assertEqual(self.run_tool("--confirm-gfx103", "--", "true").returncode, 2)
        self.data["vulkan"]["devices"].pop()
        self.env["DRI_PRIME"] = "1"
        self.assertEqual(self.run_tool("--confirm-gfx103", "--", "true").returncode, 2)

    def test_steam_session_signal_depends_on_installed_route(self):
        env = self.env.copy()
        env["UGM_RUNTIME_DIR"] = str(Path(self.tmp.name) / "no-runtime")
        env["UGM_DEV_BACKEND"] = "sdl"
        env["XDG_RUNTIME_DIR"] = self.tmp.name
        env.pop("RADV_FORCE_VRS_CONFIG_FILE", None)
        script = f'source "{STEAM_CONFIG}"; UGM_VRS_ROUTE=none; post_gamescope_start; printf "%s" "${{RADV_FORCE_VRS_CONFIG_FILE:-}}"'
        off = subprocess.run(["bash", "-c", script], env=env, capture_output=True, text=True)
        self.assertEqual(off.returncode, 0, off.stderr)
        self.assertEqual(off.stdout, "")
        hidden = subprocess.run(["bash", "-c", f'source "{STEAM_CONFIG}"; '
                                 'UGM_VRS_ROUTE=none; post_gamescope_start; '
                                 'printf "%s|%s" "${STEAM_USE_DYNAMIC_VRS:-}" '
                                 '"${RADV_FORCE_VRS_CONFIG_FILE:-}"'],
                                env={**env, "STEAM_USE_DYNAMIC_VRS": "1",
                                     "RADV_FORCE_VRS_CONFIG_FILE": "/tmp/old-vrs"},
                                capture_output=True, text=True)
        self.assertEqual(hidden.stdout, "|")
        script = (f'source "{STEAM_CONFIG}"; UGM_VRS_ROUTE=radv; post_gamescope_start; '
                  'printf "%s|%s" "${RADV_FORCE_VRS_CONFIG_FILE:-}" "${STEAM_USE_DYNAMIC_VRS:-}"')
        on = subprocess.run(["bash", "-c", script], env=env, capture_output=True, text=True)
        self.assertEqual(on.returncode, 0, on.stderr)
        path, signal = on.stdout.split("|", 1)
        self.assertEqual(Path(path).read_text(), "1x1\n")
        self.assertEqual(signal, "1")

    def test_generation_confirmation_required_for_run(self):
        self.assertEqual(self.run_tool("--", "true").returncode, 2)
        self.assertEqual(self.run_tool("--probe").returncode, 0)


if __name__ == "__main__":
    unittest.main()
