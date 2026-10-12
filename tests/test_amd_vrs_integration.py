#!/usr/bin/env python3
"""Contrato de selección, staging y señal QAM para la ruta AMD."""
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SELECT = ROOT / "src/ugm-vrs-select"
STAGE = ROOT / "src/ugm-vrs-stage"
ACTIVATE = ROOT / "src/ugm-vrs-activate"
POSTINST = ROOT / "packaging/DEBIAN/postinst"
STEAM = ROOT / "system/gamescope-session/steam"


def load_script(name, path):
    spec = importlib.util.spec_from_loader(name, importlib.machinery.SourceFileLoader(name, str(path)))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class AmdVrsIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.layer = self.root / "vrs"
        self.layer.mkdir()
        for route in ("amd", "nvidia"):
            for arch in ("32", "64"):
                (self.layer / f"libugm_{route}_vrs_{arch}.so").write_bytes(b"test")
                (self.layer / f"VK_LAYER_UGM_{route}_vrs_{arch}.json").write_text(json.dumps({"layer": {
                    "name": f"VK_LAYER_UGM_{route}_vrs", "library_path": "old"}}))
        self.selector = load_script("ugm_vrs_select_test", SELECT)
        self.data = {
            "relation": {"state": "matched"},
            "vulkan": {"selected": {"deviceUUID": "gpu-a", "driverID": "DRIVER_ID_MESA_RADV",
                                    "gpu_pci_id": "1002:15bf",
                                    "deviceType": "PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU"},
                       "devices": [{"deviceUUID": "gpu-a", "software": False}]},
            "capabilities": {"radv_vrs": {"state": "present_unvalidated"}}}

    def test_modern_amd_selected_without_apu_restriction(self):
        self.assertEqual(self.selector.select_route(self.data, self.layer), "amd")
        self.data["vulkan"]["selected"]["gpu_pci_id"] = "1002:73bf"
        self.assertEqual(self.selector.select_route(self.data, self.layer), "amd")

    def test_original_radv_apu_is_automatic_without_layer(self):
        self.data["vulkan"]["selected"]["gpu_pci_id"] = "1002:163f"
        self.assertEqual(self.selector.select_route(self.data, self.layer), "radv")
        self.data["vulkan"]["selected"]["gpu_pci_id"] = "1002:1435"
        self.assertEqual(self.selector.select_route(self.data, self.layer), "radv")
        self.data["vulkan"]["selected"]["gpu_pci_id"] = "1002:1681"
        self.assertEqual(self.selector.select_route(self.data, self.layer), "radv")
        self.data["vulkan"]["selected"]["deviceType"] = "PHYSICAL_DEVICE_TYPE_DISCRETE_GPU"
        self.assertEqual(self.selector.select_route(self.data, self.layer), "amd")

    def test_layer_installed_only_for_accepted_route(self):
        installer = load_script("ugm_vrs_activate_test", ACTIVATE)
        active = self.root / "active"
        installer.activate("amd", self.layer, active)
        self.assertEqual({p.name for p in active.iterdir()}, set(installer.filenames("amd")))
        installer.activate("none", self.layer, active)
        self.assertEqual(list(active.iterdir()), [])
        installer.activate("nvidia", self.layer, active)
        self.assertEqual({p.name for p in active.iterdir()}, set(installer.filenames("nvidia")))
        installer.activate("amd", self.layer, active)
        self.assertEqual({p.name for p in active.iterdir()}, set(installer.filenames("amd")))
        installer.activate("radv", self.layer, active)
        self.assertEqual(list(active.iterdir()), [])

    def test_installer_prompt_matrix(self):
        source = POSTINST.read_text()
        section = "# Detectar primero." + source.split("# Detectar primero.", 1)[1].split('vrs_config="', 1)[0]
        for detected, answer, installed, prompt in (
                ("radv", "false", "radv", ""),
                ("amd", "true", "amd", "ubuntu-gaming-mode/amd-vrs-experimental"),
                ("amd", "false", "none", "ubuntu-gaming-mode/amd-vrs-experimental"),
                ("nvidia", "true", "nvidia", "ubuntu-gaming-mode/nvidia-vrs-experimental"),
                ("nvidia", "false", "none", "ubuntu-gaming-mode/nvidia-vrs-experimental"),
                ("none", "true", "none", "")):
            script = '''set -e
            die() { exit 99; }
            log() { :; }
            db_fset() { :; }
            db_input() { printf '%s\n' "$2" >> "$TRACE"; }
            db_go() { :; }
            db_get() { RET="$ANSWER"; }
            function /usr/lib/ubuntu-gaming-mode/ugm-vrs-select() { printf '%s\n' "$DETECTED"; }
            function /usr/lib/ubuntu-gaming-mode/ugm-vrs-activate() { printf 'installed:%s\n' "$1" >> "$TRACE"; }
            ''' + section + '\nprintf "route:%s\n" "$vrs_route" >> "$TRACE"\n'
            trace = self.root / "trace"
            trace.write_text("")
            result = subprocess.run(["bash", "-c", script], env={**os.environ,
                 "DETECTED": detected, "ANSWER": answer, "TRACE": str(trace)},
                 capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            lines = trace.read_text().splitlines()
            self.assertEqual(lines[-2:], [f"installed:{installed}", f"route:{installed}"])
            self.assertEqual(lines[:-2], [prompt] if prompt else [])

    def test_ambiguous_or_unsupported_hardware_stays_off(self):
        self.data["vulkan"]["devices"].append({"deviceUUID": "gpu-b", "software": False})
        self.assertEqual(self.selector.select_route(self.data, self.layer), "none")
        self.data["vulkan"]["devices"].pop()
        self.data["capabilities"]["radv_vrs"]["state"] = "unknown"
        self.assertEqual(self.selector.select_route(self.data, self.layer), "none")
        self.data["capabilities"]["radv_vrs"]["state"] = "present_unvalidated"
        (self.layer / "libugm_amd_vrs_32.so").unlink()
        self.assertEqual(self.selector.select_route(self.data, self.layer), "none")

    def test_stage_rewrites_both_routes_from_readable_payload(self):
        stage = load_script("ugm_vrs_stage_test", STAGE)
        # El staging de sesión no puede atravesar /var/lib/ubuntu-gaming-mode (0700).
        self.assertEqual(stage.SOURCE, Path("/usr/lib/ubuntu-gaming-mode/vrs-bundled"))
        stage.SOURCE = self.layer
        import sys
        from unittest.mock import patch
        target = self.root / ".local/share/ubuntu-gaming-mode/vrs"
        for route in ("amd", "nvidia"):
            with patch.object(sys, "argv", [str(STAGE), route]), \
                 patch.dict(os.environ, {"HOME": str(self.root)}):
                self.assertEqual(stage.main(), 0)
            for arch in ("32", "64"):
                library = target / f"libugm_{route}_vrs_{arch}.so"
                manifest = json.loads((target / f"VK_LAYER_UGM_{route}_vrs_{arch}.json").read_text())
                self.assertEqual(library.read_bytes(), b"test")
                self.assertEqual(manifest["layer"]["library_path"], str(library))

    def test_qam_file_and_selected_layer_exported(self):
        # Interceptar el ejecutable absoluto de staging sin tocar /usr.
        for route in ("amd", "nvidia"):
            with self.subTest(route=route):
                script = f'''function /usr/lib/ubuntu-gaming-mode/ugm-vrs-stage() {{
                    printf '%s\\n' '{self.layer}'
                }}
                source '{STEAM}'
                UGM_VRS_ROUTE={route}
                post_gamescope_start
                printf '%s|%s|%s|%s' "$RADV_FORCE_VRS_CONFIG_FILE" "$UGM_VRS_CONFIG_FILE" \
                    "$VK_INSTANCE_LAYERS" "$STEAM_USE_DYNAMIC_VRS"
                '''
                env = os.environ.copy()
                env.update(HOME=str(self.root), UGM_DEV_BACKEND="sdl", UGM_RUNTIME_DIR=str(self.root))
                result = subprocess.run(["bash", "-c", script], env=env, text=True,
                                        capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                radv_file, ugm_file, layers, signal = result.stdout.split("|")
                self.assertEqual(radv_file, ugm_file)
                self.assertEqual(Path(radv_file).read_text(), "1x1\n")
                self.assertIn(f"VK_LAYER_UGM_{route}_vrs", layers)
                self.assertEqual(signal, "1")


if __name__ == "__main__":
    unittest.main()
