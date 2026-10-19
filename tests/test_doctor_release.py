"""Regression checks for the 2.8 Doctor, including genuine corruption detection."""
import hashlib
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class DoctorReleaseTests(unittest.TestCase):
    def test_package_state_and_cache_versions_agree(self):
        version = re.search(r'^Version: (.+)$', (ROOT/'packaging/DEBIAN/control').read_text(), re.M)[1]
        for name in ('preinst', 'postinst'):
            self.assertIn(f'PKG_VERSION="{version}"', (ROOT/'packaging/DEBIAN'/name).read_text())
        for name in ('src/ubuntu-gaming-mode-doctor', 'system/gamescope-session/steam', 'system/autostart/ugm-inventory.desktop'):
            versions = re.findall(r'--runtime-version ([\w.-]+)', (ROOT/name).read_text())
            self.assertTrue(versions)
            self.assertEqual(set(versions), {version})

    def test_runtime_hash_and_build_gate_agree(self):
        doctor = (ROOT/'src/ubuntu-gaming-mode-doctor').read_text()
        expected = re.search(r'EXPECTED_GAMESCOPE_SHA="([a-f0-9]+)"', doctor)[1]
        self.assertIn(f'="{expected}"', (ROOT/'packaging/build-deb.sh').read_text())
        runtime = re.search(r'EXPECTED_RUNTIME_SHA="([a-f0-9]+)"', doctor)[1]
        self.assertEqual(runtime, hashlib.sha256((ROOT/'src/gamescope-session-plus-runtime').read_bytes()).hexdigest())

    def test_current_gamescope_no_false_warning_but_corruption_fails(self):
        source = (ROOT/'src/ubuntu-gaming-mode-doctor').read_text()
        section = source.split('section "GAMESCOPE"', 1)[1].split('if [ -x "$RUNTIME/ugm-system-gamescope-link" ]', 1)[0]
        version = '3.16.30-9-g2dd5d3b0+'
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp)/'gamescope'
            binary.write_text(f'#!/bin/sh\necho "gamescope version {version} (gcc 15.2.0)"\n')
            binary.chmod(0o755)
            digest = hashlib.sha256(binary.read_bytes()).hexdigest()
            prefix = f'RUNTIME="{tmp}"\nEXPECTED_GAMESCOPE_SHA={digest}\n'
            prefix += 'ok() { echo "OK $*"; }; fail() { echo "FAIL $*"; }; warn() { echo "WARN $*"; };\n'
            def run():
                result = subprocess.run(['bash', '-uc', prefix+section], text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                return result.stdout
            self.assertNotIn('WARN', run())
            self.assertNotIn('FAIL', run())
            binary.write_text(binary.read_text()+'# modified\n')
            self.assertIn('FAIL SHA256 Gamescope NO coincide', run())

if __name__ == '__main__':
    unittest.main()
