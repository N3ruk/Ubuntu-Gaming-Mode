"""Regressions for the Decky umask and plugin-worker permissions."""
import hashlib
import io
import os
from pathlib import Path
import tempfile
import types
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
# The helper has no .py suffix, so load source explicitly.
MODULE = types.ModuleType("ugm_decky_integration")
exec(compile((ROOT / "src/ugm-decky-integration").read_text(), "ugm-decky-integration", "exec"), MODULE.__dict__)


class DeckyIntegrationTests(unittest.TestCase):
    def test_official_installer_uses_public_umask_and_repairs_loader(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            installer = b"#!/bin/bash\nexit 0\n"
            def run(argv, **kwargs):
                self.assertEqual(kwargs["umask"], 0o022)
                self.assertEqual(kwargs["env"]["SUDO_USER"], "n3ruk")
                loader = home / "homebrew/services/PluginLoader"
                loader.parent.mkdir(parents=True)
                loader.write_bytes(b"loader")
                loader.chmod(0o700)
                return types.SimpleNamespace(returncode=0)
            with mock.patch.object(MODULE, "SCRIPT_SHA256", hashlib.sha256(installer).hexdigest()), \
                 mock.patch.object(MODULE.urllib.request, "urlopen", return_value=io.BytesIO(installer)), \
                 mock.patch.object(MODULE.shutil, "which", return_value="/usr/bin/tool"), \
                 mock.patch.object(MODULE.subprocess, "run", side_effect=run):
                self.assertTrue(MODULE.install_loader(home, "n3ruk"))
            self.assertTrue(MODULE.prepare_loader_permissions(home))
            self.assertEqual((home / "homebrew/services/PluginLoader").stat().st_mode & 0o777, 0o755)

    def migrate(self, home, actions=None, state='active'):
        with mock.patch.object(MODULE, 'PAYLOAD', ROOT/'integrations/decky'), \
             mock.patch.object(MODULE, 'loader_state', return_value=state), \
             mock.patch.object(MODULE, 'service', side_effect=actions or (lambda action: True)) as service:
            result = MODULE.install_plugin(home, 'ugm-options', os.getuid(), os.getgid())
            return result, service.call_args_list

    def legacy(self, home):
        import json
        plugins = home/'homebrew/plugins'
        plugins.mkdir(parents=True, exist_ok=True)
        for slug, label in MODULE.LEGACY.items():
            path = plugins/slug
            path.mkdir()
            (path/'plugin.json').write_text(json.dumps({'name': label}))
            (path/'custom.txt').write_text('preserve retired code')
            settings = home/'homebrew/settings'/slug
            settings.mkdir(parents=True)
            (settings/'settings.json').write_text('original preferences')
        other = plugins/'unrelated'
        other.mkdir()
        (other/'main.py').write_text('untouched')
        return plugins

    def test_four_legacy_plugins_retired_settings_and_others_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            plugins = self.legacy(home)
            with mock.patch.object(MODULE.tempfile, 'mkdtemp', wraps=MODULE.tempfile.mkdtemp) as staging:
                success, calls = self.migrate(home)
            self.assertTrue(success)
            self.assertNotIn('plugins', Path(staging.call_args.kwargs['dir']).parts[-1:])
            self.assertEqual(calls, [mock.call('stop'), mock.call('start')])
            self.assertEqual({p.name for p in plugins.iterdir()}, {'ugm-options', 'unrelated'})
            for slug in MODULE.LEGACY:
                self.assertEqual((home/'homebrew/settings'/slug/'settings.json').read_text(), 'original preferences')
                self.assertTrue(list((home/'homebrew/ugm-options-migration').glob('retired-*/'+slug+'/custom.txt')))
            self.assertEqual((plugins/'unrelated/main.py').read_text(), 'untouched')
            target = plugins/'ugm-options'
            for path in target.rglob('*'):
                self.assertFalse(path.is_symlink())
                relative = str(path.relative_to(target))
                self.assertEqual(path.stat().st_mode & 0o777, 0o755 if path.is_dir() or relative in MODULE.EXECUTABLES else 0o644)

    def test_start_failure_rolls_back_all_plugins(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            plugins = self.legacy(home)
            success, calls = self.migrate(home, [True, False, True, True])
            self.assertFalse(success)
            self.assertFalse((plugins/'ugm-options').exists())
            self.assertEqual({p.name for p in plugins.iterdir()}, {*MODULE.LEGACY, 'unrelated'})
            self.assertEqual(calls, [mock.call('stop'),mock.call('start'),mock.call('stop'),mock.call('start')])

    def test_partial_retirement_failure_restores_moved_directories(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            plugins = self.legacy(home)
            original = Path.rename
            moved = 0
            def rename(path, target):
                nonlocal moved
                if path.parent == plugins:
                    moved += 1
                    if moved == 2:
                        raise OSError('simulated retirement failure')
                return original(path, target)
            with mock.patch.object(Path, 'rename', rename):
                success, calls = self.migrate(home)
            self.assertFalse(success)
            self.assertEqual(calls, [mock.call('stop'), mock.call('start')])
            self.assertEqual({p.name for p in plugins.iterdir()}, {*MODULE.LEGACY, 'unrelated'})

    def test_stop_failure_never_moves_plugins(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            plugins = self.legacy(home)
            success, _ = self.migrate(home, [False, True])
            self.assertFalse(success)
            self.assertEqual({p.name for p in plugins.iterdir()}, {*MODULE.LEGACY, 'unrelated'})

    def test_stopped_loader_is_not_started_and_repeat_is_supported(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            for _ in range(2):
                success, calls = self.migrate(home, state='inactive')
                self.assertTrue(success)
                self.assertEqual(calls, [mock.call('stop')])
            self.assertEqual([p.name for p in (home/'homebrew/plugins').iterdir()], ['ugm-options'])

    def test_renamed_legacy_detected_by_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            plugins = self.legacy(home)
            (plugins/'sharp-filter-selector').rename(plugins/'renamed-sharp')
            success, _ = self.migrate(home)
            self.assertTrue(success)
            self.assertFalse((plugins/'renamed-sharp').exists())

    def test_linked_or_unknown_legacy_preserved_without_stopping(self):
        for linked in (True, False):
            with self.subTest(linked=linked), tempfile.TemporaryDirectory() as temp:
                home = Path(temp)
                plugins = home/'homebrew/plugins'
                plugins.mkdir(parents=True)
                target = plugins/'sharp-filter-selector'
                if linked:
                    target.symlink_to(home, target_is_directory=True)
                else:
                    target.mkdir()
                    (target/'plugin.json').write_text('{"name":"someone else"}')
                success, calls = self.migrate(home)
                self.assertFalse(success)
                self.assertTrue(target.exists())
                self.assertEqual(calls, [])

    def test_custom_master_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            self.assertTrue(self.migrate(home)[0])
            path = home/'homebrew/plugins/ugm-options/main.py'
            path.write_text('custom')
            success, calls = self.migrate(home)
            self.assertFalse(success)
            self.assertEqual(path.read_text(), 'custom')
            self.assertEqual(calls, [])

    def test_corrupt_payload_does_not_remove_old_plugins(self):
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            plugins = self.legacy(home)
            with mock.patch.object(MODULE, 'payload_files', side_effect=ValueError('bad hash')):
                success, calls = self.migrate(home)
            self.assertFalse(success)
            self.assertEqual(calls, [])
            self.assertTrue((plugins/'sharp-filter-selector').exists())

    def test_options_cli_passes_only_master_and_loader_only_does_not_remove(self):
        with tempfile.TemporaryDirectory() as temp:
            account = types.SimpleNamespace(pw_dir=temp, pw_uid=1234, pw_gid=1234)
            with mock.patch.object(MODULE.os, 'geteuid', return_value=0), \
                 mock.patch.object(MODULE.pwd, 'getpwnam', return_value=account), \
                 mock.patch.object(MODULE, 'loader_present', return_value=True), \
                 mock.patch.object(MODULE, 'prepare_loader_permissions', return_value=True), \
                 mock.patch.object(MODULE, 'install_loader', return_value=True), \
                 mock.patch.object(MODULE, 'install_plugin', return_value=True) as install:
                with mock.patch.object(MODULE.sys, 'argv', ['helper', 'player', '0', '1']):
                    self.assertEqual(MODULE.main(), 0)
                install.assert_called_once_with(Path(temp), 'ugm-options', 1234, 1234)
                install.reset_mock()
                with mock.patch.object(MODULE.sys, 'argv', ['helper', 'player', '1', '0']):
                    self.assertEqual(MODULE.main(), 0)
                install.assert_not_called()


if __name__ == '__main__':
    unittest.main()
