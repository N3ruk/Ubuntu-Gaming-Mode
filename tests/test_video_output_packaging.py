"""Debconf flag wiring without running postinst or changing the host."""
import itertools
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KEYS = ('decky-loader', 'decky-options')


class VideoPackagingTests(unittest.TestCase):
    def test_loader_and_options_choices_reach_the_correct_helper_arguments(self):
        source = (ROOT/'packaging/DEBIAN/postinst').read_text()
        section = source.split('# Integración opcional: un fallo externo no bloquea la configuración de UGM.', 1)[1].split('log "Ejecutando diagnóstico final."', 1)[0]
        with tempfile.TemporaryDirectory() as temp:
            record = Path(temp)/'args'
            helper = Path(temp)/'helper'
            helper.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "'+str(record)+'"\n')
            helper.chmod(0o755)
            section = section.replace('/usr/lib/ubuntu-gaming-mode/ugm-decky-integration', str(helper))
            for flags in itertools.product((0, 1), repeat=2):
                cases = '\n'.join(f'ubuntu-gaming-mode/{key}) RET={"true" if flag else "false"} ;;' for key,flag in zip(KEYS,flags))
                harness = 'set -e\nUGM_USER=player\nwarn() { exit 55; }\ndb_get() { case "$1" in\n'+cases+'\nesac; }\n'+section
                result = subprocess.run(['sh', '-c', harness], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                if any(flags):
                    self.assertEqual(record.read_text().splitlines(), ['player', *map(str, flags)])
                else:
                    self.assertFalse(record.exists())
                if record.exists():
                    record.unlink()

    def test_master_prompt_is_default_off_bilingual_and_explains_all_modules_and_migration(self):
        config = (ROOT/'packaging/DEBIAN/config').read_text()
        self.assertIn('db_input high ubuntu-gaming-mode/decky-options', config)
        templates = (ROOT/'packaging/DEBIAN/templates').read_text()
        prompt = templates.split('Template: ubuntu-gaming-mode/decky-options\n', 1)[1]
        for text in ('Default: false', 'UGM Options', 'Description-es.UTF-8:', 'NIS/FSR', 'QAM', 'SGSR', 'resolución interna', 'monitor', 'Non-Steam', 'salida de pantalla', 'automáticamente', 'conservando sus ajustes', 'Decky Loader'):
            self.assertIn(text, prompt)

    def test_packaged_payload_contains_only_master_and_all_verified_runtime_files(self):
        import hashlib
        import json
        with tempfile.TemporaryDirectory() as temp:
            subprocess.run(['python3', str(ROOT/'packaging/install-decky-plugin.py'), str(ROOT), temp], check=True, capture_output=True)
            plugins = Path(temp)/'usr/share/ubuntu-gaming-mode/decky-plugins'
            self.assertEqual([p.name for p in plugins.iterdir()], ['ugm-options'])
            payload = plugins/'ugm-options'
            manifest = json.loads((payload/'manifest.json').read_text())
            self.assertEqual({str(p.relative_to(payload)) for p in payload.rglob('*') if p.is_file()}, {*manifest['sha256'], 'manifest.json'})
            for name, digest in manifest['sha256'].items():
                self.assertEqual(hashlib.sha256((payload/name).read_bytes()).hexdigest(), digest)
            self.assertEqual((payload/'modules/video/dist/ugm-active-display').stat().st_mode & 0o777, 0o755)
            self.assertFalse((payload/'node_modules').exists())

    def test_old_debconf_selection_migrates_once_without_overriding_seen_rejection(self):
        source = (ROOT/'packaging/DEBIAN/config').read_text()
        section = source.split('# Migrar la selección anterior',1)[1].split('db_input high ubuntu-gaming-mode/decky-options',1)[0]
        section = '# Migrar la selección anterior'+section
        for seen in ('true','false'):
            for old in ('true','false'):
                harness = 'set -e\ndb_fget() { RET='+seen+'; }\ndb_get() { RET='+old+'; }\ndb_set() { echo "$1=$2"; }\n'+section
                result = subprocess.run(['sh','-c',harness], capture_output=True,text=True,check=True)
                self.assertEqual(bool(result.stdout), seen=='false' and old=='true')
