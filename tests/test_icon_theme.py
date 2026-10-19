"""Exercise icon opt-in with a private HOME and a private D-Bus settings bus."""
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class IconThemeTests(unittest.TestCase):
    def test_enable_disable_and_manual_override(self):
        if not shutil.which('dbus-run-session') or not shutil.which('gsettings'):
            self.skipTest('D-Bus/GSettings unavailable')
        with tempfile.TemporaryDirectory() as temp:
            home = Path(temp)
            source = home / 'source' / 'UGM-ICONS'
            source.mkdir(parents=True)
            (source / 'index.theme').write_text('[Icon Theme]\nName=UGM-ICONS\n')
            (source / 'icon.svg').write_text('<svg/>\n')
            (source / 'alias.svg').symlink_to('icon.svg')
            archive = home / 'icons.tar.xz'
            with tarfile.open(archive, 'w:xz') as output:
                output.add(source, arcname='UGM-ICONS')
            env = os.environ.copy()
            env.update(HOME=temp, XDG_DATA_HOME=str(home / 'data'),
                       XDG_STATE_HOME=str(home / 'state'),
                       XDG_CONFIG_HOME=str(home / 'config'),
                       GSETTINGS_BACKEND='dconf')
            code = r'''
import pathlib, subprocess, sys
helper, archive = sys.argv[1:]
def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout.strip()
def theme():
    return run('gsettings', 'get', 'org.gnome.desktop.interface', 'icon-theme')
run('gsettings', 'set', 'org.gnome.desktop.interface', 'icon-theme', 'Yaru')
run(helper, 'enable', archive)
target = pathlib.Path.home() / 'data/icons/UGM-ICONS'
assert (target / 'index.theme').is_file()
assert (target / 'alias.svg').is_symlink()
assert theme() == "'UGM-ICONS'", theme()
run(helper, 'disable')
assert theme() == "'Yaru'", theme()
assert target.exists(), 'User files must be preserved'
run(helper, 'enable', archive)
run('gsettings', 'set', 'org.gnome.desktop.interface', 'icon-theme', 'Adwaita')
run(helper, 'disable')
assert theme() == "'Adwaita'", theme()
'''
            result = subprocess.run(['dbus-run-session', '--', 'python3', '-c', code,
                                     str(ROOT / 'src/ugm-icon-theme'), str(archive)],
                                    env=env, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_debconf_wiring(self):
        question = 'ubuntu-gaming-mode/desktop-icon-theme'
        self.assertIn(f'db_input high {question}',
                      (ROOT / 'packaging/DEBIAN/config').read_text())
        self.assertIn(f'db_get {question}',
                      (ROOT / 'packaging/DEBIAN/postinst').read_text())
        templates = (ROOT / 'packaging/DEBIAN/templates').read_text()
        self.assertIn(f'Template: {question}\nType: boolean\nDefault: false', templates)
        self.assertIn('Description-es.UTF-8: ¿Instalar y activar', templates)


if __name__ == '__main__':
    unittest.main()
