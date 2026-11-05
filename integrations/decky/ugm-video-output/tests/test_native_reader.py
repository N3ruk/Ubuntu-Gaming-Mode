"""Exercise the real compiled reader against a synthetic Wayland server."""
import os
from pathlib import Path
import select
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeReaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.work = Path(cls.temp.name)
        protocol = ROOT / 'native/gamescope-control.xml'
        subprocess.run(['wayland-scanner', 'server-header', str(protocol), str(cls.work/'gamescope-control-server-protocol.h')], check=True, capture_output=True)
        subprocess.run(['wayland-scanner', 'private-code', str(protocol), str(cls.work/'protocol.c')], check=True, capture_output=True)
        flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'wayland-server'], text=True).split()
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(cls.work), str(ROOT/'tests/fake-gamescope.c'), str(cls.work/'protocol.c'), *flags, '-o', str(cls.work/'server')], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def query(self, version=7, report=1):
        with tempfile.TemporaryDirectory() as runtime:
            env = {**os.environ, 'XDG_RUNTIME_DIR': runtime, 'GAMESCOPE_WAYLAND_DISPLAY': 'gamescope-test'}
            server = subprocess.Popen([str(self.work/'server'), str(version), str(report)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                self.assertTrue(select.select([server.stdout], [], [], 2)[0], 'fake server did not start')
                self.assertEqual(server.stdout.readline(), 'ready\n')
                return subprocess.run([str(ROOT/'dist/ugm-active-display')], env=env, capture_output=True, text=True, timeout=2)
            finally:
                server.terminate()
                _, error = server.communicate(timeout=2)
                self.assertEqual(error, '')

    def test_real_protocol_active_connector_and_version_cap(self):
        result = self.query(version=7)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'Connector Name: DP-7\n')

    def test_version_two_is_supported(self):
        self.assertEqual(self.query(version=2).returncode, 0)

    def test_missing_display_info_cannot_confirm_success(self):
        result = self.query(report=0)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')

    def test_protocol_without_display_event_is_rejected(self):
        self.assertNotEqual(self.query(version=1).returncode, 0)

    def test_no_debug_or_mutation_command_interface(self):
        result = subprocess.run([str(ROOT/'dist/ugm-active-display'), 'anything'], capture_output=True, timeout=2)
        self.assertEqual(result.returncode, 2)
