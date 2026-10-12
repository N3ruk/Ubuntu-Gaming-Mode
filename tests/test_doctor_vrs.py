#!/usr/bin/env python3
"""Prueba la auditoría VRS del Doctor sin modificar /etc."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
DOCTOR = ROOT / "src/ubuntu-gaming-mode-doctor"
STEAM = ROOT / "system/gamescope-session/steam"


class DoctorVrsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.config = self.root / "vrs.conf"
        self.active = self.root / "active"
        self.bundled = self.root / "bundled"
        self.active.mkdir()
        self.bundled.mkdir()
        self.steam = self.root / "steam"
        self.steam.write_bytes(STEAM.read_bytes())
        source = DOCTOR.read_text()
        self.section = source.split('section "VRS INSTALADO Y SEÑAL QAM"', 1)[1].split(
            '# INVENTARIO DE CAPACIDADES', 1)[0]
        self.section = 'section "VRS INSTALADO Y SEÑAL QAM"' + self.section
        self.section = self.section.replace('VRS_CONFIG="/etc/ubuntu-gaming-mode/vrs.conf"',
                                            f'VRS_CONFIG="{self.config}"')
        self.section = self.section.replace('VRS_ACTIVE="$STATE_ROOT/vrs"',
                                            f'VRS_ACTIVE="{self.active}"')
        self.section = self.section.replace('VRS_BUNDLED="$RUNTIME/vrs-bundled"',
                                            f'VRS_BUNDLED="{self.bundled}"')

    def run_check(self):
        script = ("set -u\nFAIL=0\nOK=0\nSTEAM_CONFIG=\"" + str(self.steam) +
                  "\"\nsection() { :; }\nok() { OK=$((OK+1)); }\n" +
                  "fail() { printf '%s\\n' \"$*\"; FAIL=$((FAIL+1)); }\n" +
                  self.section + '\nprintf "FAIL_COUNT=%s\n" "$FAIL"\n')
        result = subprocess.run(["bash", "-c", script], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def test_none_and_radv_require_no_active_layer(self):
        for route in ("none", "radv"):
            self.config.write_text(f"UGM_VRS_ROUTE={route}\n")
            self.assertIn("FAIL_COUNT=0", self.run_check())

    def test_accepted_layer_must_match_bundled_both_architectures(self):
        self.config.write_text("UGM_VRS_ROUTE=amd\n")
        for arch in ("32", "64"):
            for name in (f"libugm_amd_vrs_{arch}.so", f"VK_LAYER_UGM_amd_vrs_{arch}.json"):
                (self.bundled / name).write_bytes(b"valid")
                (self.active / name).write_bytes(b"valid")
        self.assertIn("FAIL_COUNT=0", self.run_check())
        (self.active / "libugm_amd_vrs_32.so").write_bytes(b"corrupt")
        self.assertIn("FAIL_COUNT=1", self.run_check())

    def test_declined_layer_leaves_no_active_files(self):
        self.config.write_text("UGM_VRS_ROUTE=none\n")
        (self.active / "libugm_amd_vrs_64.so").write_bytes(b"stale")
        self.assertIn("FAIL_COUNT=1", self.run_check())

    def test_qam_policy_missing_is_reported(self):
        self.config.write_text("UGM_VRS_ROUTE=none\n")
        self.steam.write_text("# missing hook\n")
        self.assertIn("FAIL_COUNT=1", self.run_check())


if __name__ == "__main__":
    unittest.main()
