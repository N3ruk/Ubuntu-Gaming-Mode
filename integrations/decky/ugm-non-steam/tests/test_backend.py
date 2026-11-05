import asyncio
import importlib.util
import os
from pathlib import Path
import stat
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from catalog import Catalog, exec_tokens, expand_fields, quote_arg
from shortcuts import read_vdf, saved_shortcuts
spec = importlib.util.spec_from_file_location('nonsteam_backend', ROOT / 'main.py')
backend = importlib.util.module_from_spec(spec)
spec.loader.exec_module(backend)


def vdf(value):
    out = bytearray()
    for key, item in value.items():
        if isinstance(item, dict):
            out.extend(b'\0' + key.encode() + b'\0' + vdf(item))
        elif isinstance(item, int):
            out.extend(b'\2' + key.encode() + b'\0' + struct.pack('<I', item))
        else:
            out.extend(b'\1' + key.encode() + b'\0' + item.encode() + b'\0')
    return bytes(out) + b'\x08'


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.home = Path(self.tmp.name)
        self.env = {'XDG_DATA_HOME': str(self.home / 'user'), 'XDG_DATA_DIRS': str(self.home / 'system'),
                    'LANG': 'es_ES.UTF-8', 'XDG_CURRENT_DESKTOP': 'ubuntu:GNOME'}
        self.catalog = Catalog(self.home, os.getuid(), self.env)
        self.program = self.home / 'Juego con espacios Ω.AppImage'
        self.program.write_bytes(b'\x7fELFfixture')
        self.program.chmod(0o700)

    def desktop(self, name='sample', extra='', user=True, command=None):
        root = self.home / ('user' if user else 'system') / 'applications'
        root.mkdir(parents=True, exist_ok=True)
        path = root / (name + '.desktop')
        path.write_text('[Desktop Entry]\nType=Application\nName=Example\nName[es]=Ejemplo\nExec=' +
                        (command or '"' + str(self.program) + '"') + '\n' + extra)
        return path

    def test_localized_name_and_quoted_path(self):
        app = self.catalog.application(self.desktop())
        self.assertEqual(app['name'], 'Ejemplo')
        self.assertEqual(app['exe'], str(self.program))
        self.assertEqual(app['startDir'], str(self.home))

    def test_field_codes_percent_name_and_icon(self):
        tokens = expand_fields(exec_tokens('/bin/true %U %% %c %k %i'), 'Mi Juego', Path('/a b.desktop'), 'icon')
        self.assertEqual(tokens, ['/bin/true', '%', 'Mi Juego', '/a b.desktop', '--icon', 'icon'])

    def test_flatpak_document_wrappers(self):
        tokens = expand_fields(exec_tokens('/bin/true run org.game @@u %U @@'), 'Game', Path('/app'), '')
        self.assertEqual(tokens, ['/bin/true', 'run', 'org.game'])

    def test_quoted_arguments_remain_single_tokens(self):
        self.assertEqual(exec_tokens('/bin/true "two words" ""'), ['/bin/true', 'two words', ''])
        self.assertEqual(quote_arg('a $x `id` "b"'), '"a \\$x \\`id\\` \\"b\\""')

    def test_invalid_exec_does_not_fall_back_to_split(self):
        for command in ['/bin/true "broken', '/bin/true ; touch /tmp/a', '/bin/true $(id)', '/bin/true %x']:
            self.assertIsNone(self.catalog.application(self.desktop(command=command)))

    def test_hidden_user_entry_masks_visible_system_copy(self):
        self.desktop(user=False)
        self.desktop(extra='Hidden=true\n')
        with patch('catalog.application_roots', return_value=[self.home / 'user/applications', self.home / 'system/applications']):
            self.assertEqual(self.catalog.applications()['total'], 0)

    def test_visibility_terminal_and_tryexec(self):
        for extra in ['Hidden=true', 'NoDisplay=true', 'Terminal=true', 'TryExec=not-installed-ugm',
                      'OnlyShowIn=KDE;', 'NotShowIn=GNOME;']:
            self.assertIsNone(self.catalog.application(self.desktop(extra=extra)))

    def test_desktop_working_directory_and_env_arguments(self):
        app = self.catalog.application(self.desktop(extra='Path=' + str(self.home), command='/usr/bin/env TEST=value /bin/true --flag "two words"'))
        self.assertEqual(app['exe'], '/usr/bin/env')
        self.assertEqual(app['launchOptions'], '"TEST=value" "/bin/true" "--flag" "two words"')

    def test_dbus_only_and_missing_destinations_are_excluded(self):
        self.assertIsNone(self.catalog.application(self.desktop(command='/missing/ugm')))
        path = self.desktop()
        path.write_text('[Desktop Entry]\nType=Application\nName=DBus\nDBusActivatable=true\n')
        self.assertIsNone(self.catalog.application(path))

    def test_precedence_and_nested_desktop_id(self):
        a = self.desktop(user=False)
        a.unlink()
        directory = a.parent / 'a'
        directory.mkdir()
        (directory / 'b.desktop').write_text('[Desktop Entry]\nType=Application\nName=Nested\nExec=/bin/true\n')
        self.desktop('a-b', extra='Hidden=true')
        with patch('catalog.application_roots', return_value=[self.home / 'user/applications', self.home / 'system/applications']):
            self.assertEqual(self.catalog.applications()['total'], 0)

    def test_application_pagination_and_search(self):
        self.catalog.env['LANG'] = 'en_US.UTF-8'
        for i in range(45):
            self.desktop(str(i), extra='Name=Game ' + str(i))
        with patch('catalog.application_roots', return_value=[self.home / 'user/applications']):
            self.assertEqual(len(self.catalog.applications()['items']), 40)
            self.assertEqual(len(self.catalog.applications(page=1)['items']), 5)
            self.assertGreater(self.catalog.applications('game 4')['total'], 0)

    def test_browse_folders_first_and_hidden_filter(self):
        (self.home / 'Folder').mkdir()
        (self.home / '.hidden').mkdir()
        (self.home / 'data.iso').write_text('data')
        data = self.catalog.browse(str(self.home))
        self.assertTrue(data['items'][0]['folder'])
        self.assertNotIn('.hidden', [i['name'] for i in data['items']])
        self.assertNotIn('data.iso', [i['name'] for i in data['items']])
        self.assertIn('.hidden', [i['name'] for i in self.catalog.browse(str(self.home), hidden=True)['items']])

    def test_missing_and_non_program_files(self):
        for name in ['missing', 'game.iso', 'game.exe', 'game.cmd']:
            path = self.home / name
            if name != 'missing':
                path.write_bytes(b'not a program')
            with self.assertRaises(ValueError):
                self.catalog.inspect(str(path))

    def test_noexec_unit_is_reported(self):
        with patch.object(self.catalog, 'noexec', return_value=True):
            with self.assertRaisesRegex(ValueError, 'noexec'):
                self.catalog.inspect(str(self.program))

    def test_chmod_only_explicit_adds_owner_execute(self):
        self.program.chmod(0o640)
        self.assertFalse(self.catalog.inspect(str(self.program))['executable'])
        self.assertEqual(stat.S_IMODE(self.program.stat().st_mode), 0o640)
        self.assertTrue(self.catalog.make_executable(str(self.program))['executable'])
        self.assertEqual(stat.S_IMODE(self.program.stat().st_mode), 0o740)

    def test_chmod_does_not_follow_symlink(self):
        self.program.chmod(0o600)
        link = self.home / 'link.AppImage'
        link.symlink_to(self.program)
        with self.assertRaises(OSError):
            self.catalog.make_executable(str(link))
        self.assertEqual(stat.S_IMODE(self.program.stat().st_mode), 0o600)

    def test_chmod_does_not_change_other_owners(self):
        self.catalog.uid += 10000
        self.program.chmod(0o600)
        with self.assertRaises(ValueError):
            self.catalog.make_executable(str(self.program))

    def test_windows_requires_explicit_tool(self):
        exe = self.home / 'Game.exe'
        exe.write_bytes(b'MZfixture')
        draft = self.catalog.inspect(str(exe))
        with self.assertRaisesRegex(ValueError, 'Proton'):
            self.catalog.validate(draft)
        draft['compatTool'] = 'proton_experimental'
        self.assertTrue(self.catalog.validate(draft)['windows'])

    def test_validation_rechecks_disconnected_file(self):
        draft = self.catalog.inspect(str(self.program))
        self.program.unlink()
        with self.assertRaises(ValueError):
            self.catalog.validate(draft)

    def test_text_and_relative_paths_rejected(self):
        for raw in ['relative', '/tmp/"quote"', '/tmp/a\nline', None]:
            with self.assertRaises(ValueError):
                self.catalog.inspect(raw)

    def test_user_places_are_not_sourced(self):
        (self.home / '.config').mkdir()
        (self.home / 'Downloads').mkdir()
        (self.home / '.config/user-dirs.dirs').write_text('XDG_DOWNLOAD_DIR="$HOME/Downloads"\nXDG_DESKTOP_DIR="$(touch /tmp/UGM-SHOULD-NOT-EXIST)"')
        self.assertIn(str(self.home / 'Downloads'), [p['path'] for p in self.catalog.locations()])


class VDFTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.home = Path(self.tmp.name)
        self.sid = str(76561197960265728 + 123)
        self.entry = {'appid': 0xdeadbeef, 'AppName': 'Game Ω', 'Exe': '"/a b/game"', 'StartDir': '/a b', 'LaunchOptions': '--test'}

    def put(self, root='.local/share/Steam', account='123'):
        path = self.home / root / 'userdata' / account / 'config/shortcuts.vdf'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(vdf({'shortcuts': {'0': self.entry}}))
        return path

    def test_binary_roundtrip_unsigned_id(self):
        self.put()
        data = saved_shortcuts(self.home, self.sid)
        self.assertEqual(data['items'][0]['appId'], 0xdeadbeef)
        self.assertEqual(data['items'][0]['name'], 'Game Ω')

    def test_other_account_never_used_as_fallback(self):
        self.put(account='456')
        self.assertEqual(saved_shortcuts(self.home, self.sid)['items'], [])

    def test_invalid_or_unknown_accounts(self):
        for sid in ['', '../../123', '76561197960265728', None]:
            with self.assertRaises(ValueError):
                saved_shortcuts(self.home, sid)

    def test_truncated_unknown_and_oversized_vdf(self):
        valid = vdf({'shortcuts': {'0': self.entry}})
        for data in [valid[:-1], b'\x07name\0\x08', b'\0broken', b'a' * (8 * 1024 * 1024 + 1)]:
            with self.assertRaises(ValueError):
                read_vdf(data)

    def test_multiple_steam_roots_rejected_not_guessed(self):
        self.put()
        self.put('.steam/steam')
        with self.assertRaisesRegex(ValueError, 'varias bibliotecas'):
            saved_shortcuts(self.home, self.sid)

    def test_aliases_same_installation_are_deduplicated(self):
        self.put()
        (self.home / '.steam').mkdir()
        (self.home / '.steam/steam').symlink_to(self.home / '.local/share/Steam')
        self.assertEqual(len(saved_shortcuts(self.home, self.sid)['items']), 1)

    def test_diagnostics_do_not_write_steam_files(self):
        path = self.put()
        before = (path.read_bytes(), path.stat().st_mtime_ns)
        saved_shortcuts(self.home, self.sid)
        self.assertEqual(before, (path.read_bytes(), path.stat().st_mtime_ns))


class AdapterTests(unittest.IsolatedAsyncioTestCase):
    async def test_root_is_rejected(self):
        with patch.object(backend.os, 'geteuid', return_value=0):
            result = await backend.Plugin().get_context()
        self.assertFalse(result['ok'])
        self.assertIn('root', result['error'])

    async def test_rpc_reports_missing_path_not_traceback(self):
        result = await backend.Plugin().inspect_path('/missing/ugm-program')
        self.assertFalse(result['ok'])
        self.assertIn('no existe', result['error'])

    async def test_load_unload_have_no_side_effects(self):
        plugin = backend.Plugin()
        with patch.object(plugin, '_service', side_effect=AssertionError('unexpected scan')):
            await plugin._main()
            await plugin._unload()


if __name__ == '__main__':
    unittest.main()
