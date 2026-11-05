"""UGM port boundaries: synthetic procfs, no host display commands or writes."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch, Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ugm_session as session
from video_selector import Access, Boundary


class SessionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.proc = Path(self.temp.name)
        self.addCleanup(patch.stopall)
        patch.object(session, 'PROC', self.proc).start()
        self.create(4000, 1, ['/bin/bash', session.WRAPPER, 'steam'])
        self.create(4001, 4000, [session.RUNTIME, '--backend', 'drm', '--steam'], exe=session.RUNTIME)
        self.create(4002, 4000, ['steam'], env={'DISPLAY': ':7', 'GAMESCOPE_WAYLAND_DISPLAY': 'gamescope-0'})
        self.request = {'pid': 4001, 'start': '1234', 'uid': os.getuid()}

    def create(self, pid, parent, args, exe='/usr/bin/steam', env=None):
        entry = self.proc / str(pid)
        entry.mkdir(exist_ok=True)
        (entry / 'exe').symlink_to(exe)
        fields = ['S', str(parent)] + ['0'] * 17 + ['1234']
        (entry / 'stat').write_text(f'{pid} (process with spaces) ' + ' '.join(fields))
        (entry / 'cmdline').write_bytes(b'\0'.join(s.encode() for s in args) + b'\0')
        (entry / 'environ').write_bytes(b'\0'.join(f'{k}={v}'.encode() for k, v in (env or {}).items()))
        return entry

    def test_only_ugm_drm_compositor_is_recognized(self):
        self.assertEqual(session.gaming_context(), self.request)
        self.assertTrue(session.session_matches(self.request))
        self.assertFalse(session.session_matches({**self.request, 'start': 'old'}))
        self.assertFalse(session.session_matches({**self.request, 'uid': os.getuid() + 1}))

    def test_nested_backend_or_foreign_binary_cannot_be_a_gaming_session(self):
        entry = self.proc / '4001'
        (entry / 'cmdline').write_bytes(b'gamescope\0--backend\0sdl\0--steam\0')
        self.assertIsNone(session.gaming_context())
        (entry / 'cmdline').write_bytes(b'gamescope\0--backend\0drm\0--steam\0')
        with patch.object(session.os, 'readlink', return_value='/usr/bin/gamescope'):
            self.assertIsNone(session.gaming_context())

    def test_only_runtime_descendants_supply_environment(self):
        self.create(5000, 1, ['other'], env={'DISPLAY': ':99', 'GAMESCOPE_WAYLAND_DISPLAY': 'gamescope-fake'})
        candidates = session.session_environment(self.request)
        self.assertEqual(candidates, [{'DISPLAY': ':7', 'GAMESCOPE_WAYLAND_DISPLAY': 'gamescope-0'}])

    def test_missing_environment_or_unrecognized_wrapper_fails_closed(self):
        (self.proc / '4002/environ').write_bytes(b'')
        with self.assertRaisesRegex(RuntimeError, 'not ready'):
            session.session_environment(self.request)
        (self.proc / '4000/cmdline').write_bytes(b'/bin/bash\0other-wrapper\0')
        with self.assertRaisesRegex(RuntimeError, 'wrapper'):
            session.session_environment(self.request)

    def test_replaced_session_cannot_supply_environment(self):
        with self.assertRaisesRegex(RuntimeError, 'replaced'):
            session.session_environment({**self.request, 'start': 'old'})

    def test_discovered_uid_is_not_assumed_to_be_1000(self):
        with patch.object(session.Path, 'stat', return_value=Mock(st_uid=2345)):
            current = session.process_context(self.proc / '4001')
        self.assertEqual(current['uid'], 2345)


class EnvironmentTests(unittest.TestCase):
    def test_non_socket_and_escaping_authority_are_rejected_before_xprop(self):
        access = Access.__new__(Access)
        access.env = {}
        access.command = Mock(side_effect=AssertionError('must not call xprop'))
        with tempfile.TemporaryDirectory() as temp:
            runtime = Path(temp)
            (runtime / 'gamescope-0').write_bytes(b'not a socket')
            with self.assertRaises(Boundary):
                access.connect_environment(runtime, {'DISPLAY': ':7', 'GAMESCOPE_WAYLAND_DISPLAY': 'gamescope-0'})
            with patch.object(Path, 'is_socket', return_value=True):
                with self.assertRaises(Boundary):
                    access.connect_environment(runtime, {'DISPLAY': ':7', 'GAMESCOPE_WAYLAND_DISPLAY': 'gamescope-0', 'XAUTHORITY': '/tmp/foreign'})
        access.command.assert_not_called()

    def test_primary_server_proof_precedes_native_active_display_query(self):
        access = Access.__new__(Access)
        access.env = {'XAUTHORITY': '/stale'}
        access.active = Mock(return_value='DP-2')
        access.command = Mock(return_value='GAMESCOPE_XWAYLAND_SERVER_ID(CARDINAL) = 1')
        with patch.object(Path, 'is_socket', return_value=True):
            with self.assertRaises(Boundary):
                access.connect_environment(Path('/run/user/1234'), {'DISPLAY': ':7', 'GAMESCOPE_WAYLAND_DISPLAY': 'gamescope-0'})
            access.active.assert_not_called()
            access.command.return_value = 'GAMESCOPE_XWAYLAND_SERVER_ID(CARDINAL) = 0'
            self.assertEqual(access.connect_environment(Path('/run/user/1234'), {'DISPLAY': ':7', 'GAMESCOPE_WAYLAND_DISPLAY': 'gamescope-0'}), 'DP-2')
        self.assertNotIn('XAUTHORITY', access.env)
