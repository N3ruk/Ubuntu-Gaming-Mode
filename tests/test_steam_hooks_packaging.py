"""Stage real hook payloads and exercise lifecycle with HOME/systemctl isolated."""
import hashlib
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
HOOKS = ('steam-youtube-pause-hook', 'steam-client-update-hook')
loader = importlib.machinery.SourceFileLoader('ugm_steam_hooks', str(ROOT/'src/ugm-steam-hooks'))
helper = importlib.util.module_from_spec(importlib.util.spec_from_loader(loader.name, loader))
loader.exec_module(helper)


class SteamHooksPackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.env = patch.dict(os.environ, {'HOME': str(self.home), 'XDG_CONFIG_HOME': str(self.home/'config'), 'XDG_STATE_HOME': str(self.home/'state')})
        self.env.start()
        self.addCleanup(self.env.stop)
        self.systemctl = patch.object(helper, 'systemctl')
        self.ctl = self.systemctl.start()
        self.addCleanup(self.systemctl.stop)
        self.payload = patch.object(helper, 'PAYLOAD', ROOT/'tools')
        self.payload.start()
        self.addCleanup(self.payload.stop)

    def test_stage_exact_runtime_modules_contracts_and_units_without_observers(self):
        dest = self.home/'stage'
        subprocess.run([str(ROOT/'packaging/install-steam-hooks.sh'), str(ROOT), str(dest)], check=True, capture_output=True)
        for hook in HOOKS:
            target = dest/'usr/lib/ubuntu-gaming-mode/steam-hooks'/hook
            for original in (ROOT/'tools'/hook).glob('*.mjs'):
                staged = target/original.name
                if original.name.endswith('.test.mjs'):
                    self.assertFalse(staged.exists())
                else:
                    self.assertEqual(hashlib.sha256(staged.read_bytes()).digest(), hashlib.sha256(original.read_bytes()).digest())
            unit = (dest/'usr/lib/systemd/user'/f'ugm-{hook}.service').read_text()
            self.assertIn('PartOf=gamescope-session-plus@steam.service', unit)
            self.assertNotIn('WantedBy=default.target', unit)
            self.assertNotIn('/home/n3ruk/', unit)
        contract = dest/'usr/lib/ubuntu-gaming-mode/steam-hooks/steam-youtube-pause-hook/contract.json'
        self.assertEqual(contract.read_bytes(), (ROOT/'tools/steam-youtube-pause-hook/contract.json').read_bytes())
        self.assertFalse(any(dest.rglob('youtube-observer')))
        self.assertTrue((dest/'usr/share/doc/ubuntu-gaming-mode/docs/ugm-2.8.5-preparation.md').is_file())
        self.assertTrue((dest/'usr/share/doc/ubuntu-gaming-mode/docs/es/ugm-2.8.5/PREPARACION-BUILD.md').is_file())
        compile((dest/'usr/lib/ubuntu-gaming-mode/ugm-steam-hooks').read_text(), 'ugm-steam-hooks', 'exec')

    def test_owned_marker_created_private_and_cleaned_idempotently(self):
        helper.prepare()
        marker = helper.marker_file()
        self.assertEqual(marker.stat().st_mode & 0o777, 0o600)
        helper.prepare()
        helper.cleanup_marker()
        self.assertFalse(marker.exists())
        helper.cleanup_marker()

    def test_preexisting_marker_is_not_owned_or_removed(self):
        marker = helper.marker_file()
        marker.parent.mkdir(parents=True)
        marker.touch()
        helper.prepare()
        self.assertFalse(helper.state_file().exists())
        helper.cleanup_marker()
        self.assertTrue(marker.exists())

    def test_modified_replaced_or_symlinked_owned_marker_is_preserved(self):
        for mutation in ('modified', 'replaced', 'symlinked'):
            with self.subTest(mutation=mutation):
                helper.prepare()
                marker = helper.marker_file()
                if mutation == 'modified':
                    marker.write_text('user change')
                elif mutation == 'replaced':
                    replacement = marker.with_suffix('.replacement')
                    replacement.touch()
                    replacement.replace(marker)
                else:
                    marker.unlink()
                    target = self.home/'user-marker'
                    target.touch()
                    marker.symlink_to(target)
                helper.cleanup_marker()
                self.assertTrue(marker.exists())
                marker.unlink()

    def make_legacy(self, hook):
        unit = helper.config_dir()/f'ugm-{hook}.service'
        unit.parent.mkdir(parents=True, exist_ok=True)
        template = (ROOT/'tools'/hook/f'ugm-{hook}.service.in').read_text()
        unit.write_text(template.replace('@NODE@', '"/usr/bin/node"').replace('@SERVICE@', f'"/some checkout/tools/{hook}/service.mjs"'))
        link = unit.parent/'default.target.wants'/unit.name
        link.parent.mkdir(exist_ok=True)
        link.symlink_to(unit)
        return unit, link

    def test_exact_legacy_units_stopped_and_migrated_together(self):
        records = [self.make_legacy(hook) for hook in HOOKS]
        helper.migrate()
        self.assertEqual(self.ctl.call_args_list[0].args, ('stop', *(unit.name for unit, _ in records)))
        self.assertEqual(self.ctl.call_args_list[-1].args, ('daemon-reload',))
        for unit, link in records:
            self.assertFalse(os.path.lexists(unit))
            self.assertFalse(os.path.lexists(link))

    def test_custom_or_masked_override_preserves_both_and_never_stops_services(self):
        first, _ = self.make_legacy(HOOKS[0])
        second, _ = self.make_legacy(HOOKS[1])
        second.write_text(second.read_text()+'# user modification\n')
        with self.assertRaises(RuntimeError):
            helper.prepare()
        self.assertTrue(first.exists())
        self.ctl.assert_not_called()
        second.unlink()
        second.symlink_to('/dev/null')
        with self.assertRaises(RuntimeError):
            helper.prepare()
        self.assertTrue(first.exists())
        self.ctl.assert_not_called()

    def test_failed_stop_does_not_delete_legacy_units(self):
        unit, link = self.make_legacy(HOOKS[0])
        self.ctl.side_effect = subprocess.CalledProcessError(1, ['systemctl'])
        with self.assertRaises(subprocess.CalledProcessError):
            helper.migrate()
        self.assertTrue(unit.exists())
        self.assertTrue(link.exists())

    def test_nonsteam_instances_do_nothing(self):
        with patch('sys.argv', ['ugm-steam-hooks', 'prepare', 'other']):
            helper.main()
        self.ctl.assert_not_called()
        self.assertFalse(helper.marker_file().exists())

    def test_start_is_nonblocking_and_stop_cleans_owned_marker(self):
        helper.prepare()
        with patch('sys.argv', ['ugm-steam-hooks', 'start', 'steam']):
            helper.main()
        self.assertEqual(self.ctl.call_args.args, ('--no-block', 'start', *(f'ugm-{h}.service' for h in HOOKS)))
        with patch('sys.argv', ['ugm-steam-hooks', 'stop', 'steam']):
            helper.main()
        self.assertFalse(helper.marker_file().exists())

    def test_remove_cleans_owned_marker_even_when_user_manager_is_unavailable(self):
        for action in ('stop', 'remove'):
            with self.subTest(action=action):
                helper.prepare()
                self.ctl.side_effect = subprocess.CalledProcessError(1, ['systemctl'])
                with patch('sys.argv', ['ugm-steam-hooks', action, 'steam']):
                    with self.assertRaises(subprocess.CalledProcessError):
                        helper.main()
                self.assertFalse(helper.marker_file().exists())

    def test_authorized_release_manifest_and_dependency_are_explicit(self):
        release = json.loads((ROOT/'packaging/release-2.8.5.json').read_text())
        self.assertEqual(release['version'], '2.8.5-1')
        self.assertIs(release['build_enabled'], True)
        self.assertEqual(release['status'], 'release')
        self.assertEqual(set(release['components']), {*HOOKS, 'ugm-options'})
        control = (ROOT/'packaging/DEBIAN/control').read_text()
        self.assertIn('nodejs (>= 22)', control)
        recipe = (ROOT/'packaging/build-deb.sh').read_text()
        self.assertLess(recipe.index('PYRELEASE'), recipe.index('rm -rf "$WORK"'))
        self.assertIn('packaging/install-steam-hooks.sh', recipe)


if __name__ == '__main__':
    unittest.main()
