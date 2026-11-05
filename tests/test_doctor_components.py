"""Doctor 2.8.5 diagnoses fixtures without executing hooks or mutating sessions."""
import datetime
import hashlib
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
loader = importlib.machinery.SourceFileLoader('doctor_components', str(ROOT/'src/ugm-doctor-components'))
doctor = importlib.util.module_from_spec(importlib.util.spec_from_loader(loader.name, loader))
loader.exec_module(doctor)


class DoctorComponentsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.runtime = self.base/'usr/lib/ubuntu-gaming-mode'
        self.units = self.base/'usr/lib/systemd/user'
        subprocess.run([str(ROOT/'packaging/install-steam-hooks.sh'), str(ROOT), str(self.base)], check=True, capture_output=True)
        self.options = self.base/'usr/share/ubuntu-gaming-mode/decky-plugins/ugm-options'
        subprocess.run(['python3', str(ROOT/'packaging/install-decky-plugin.py'), str(ROOT), str(self.base)], check=True, capture_output=True)
        for path in self.units.glob('*.service'):
            path.write_text(path.read_text().replace('/usr/lib/ubuntu-gaming-mode', str(self.runtime)))
        path = self.units/'gamescope-session-plus@.service'
        path.write_text((ROOT/'system/systemd/gamescope-session-plus@.service').read_text().replace('/usr/lib/ubuntu-gaming-mode', str(self.runtime)))
        self.md5 = self.base/'md5sums'
        self.seal()
        self.home = self.base/'home/player'
        self.home.mkdir(parents=True)
        self.account = SimpleNamespace(pw_dir=str(self.home), pw_uid=1000, pw_name='player')
        self.gaming = False
        self.bus = True
        self.node = 'v22.1.0'
        self.overrides = {}
        self.calls = []
        self.now = datetime.datetime(2026, 11, 3, 18, 0, tzinfo=datetime.timezone.utc)

    def seal(self):
        paths = [p for base in (self.runtime, self.units, self.options) if base.exists() for p in base.rglob('*') if p.is_file()]
        self.md5.write_text(''.join(f'{hashlib.md5(p.read_bytes()).hexdigest()}  {str(p).lstrip("/")}\n' for p in paths))

    def run_command(self, argv):
        self.calls.append(argv)
        if argv == ['/usr/bin/node', '--version']:
            return self.node
        if argv == ['systemctl', 'show', 'plugin_loader.service', '--property=ActiveState', '--value']:
            return 'active'
        self.assertEqual(argv[:4], ['runuser', '-u', 'player', '--'])
        self.assertIn('show', argv)
        self.assertFalse(set(('start','stop','restart','enable','daemon-reload')) & set(argv))
        if not self.bus:
            return None
        unit = argv[-2]
        values = {'LoadState':'loaded', 'ActiveState':'active' if self.gaming else 'inactive',
                  'FragmentPath':str(self.units/unit), 'DropInPaths':'', 'UnitFileState':'static'}
        values.update(self.overrides.get(unit, {}))
        return '\n'.join(f'{k}={v}' for k,v in values.items())

    def diagnose(self, account=True):
        events = []
        doctor.diagnose(lambda level, message: events.append((level,message)), self.account if account else None,
                        runtime=self.runtime, units=self.units, options=self.options, md5file=self.md5,
                        run=self.run_command, now=self.now)
        return events

    def status(self, hook_name, **values):
        path = self.home/'.local/state/ubuntu-gaming-mode'/hook_name/'status.json'
        path.parent.mkdir(parents=True, exist_ok=True)
        data = {'time':self.now.isoformat(), 'state':'attached', 'hook':{'active':True} if hook_name=='steam-youtube-pause-hook' else {'installedBuild':1792618285, 'version':'2', 'lastError':None}}
        data.update(values)
        path.write_text(json.dumps(data))

    def attach(self):
        self.gaming = True
        marker = self.home/'.local/share/Steam/.cef-enable-remote-debugging'
        marker.parent.mkdir(parents=True)
        marker.touch()
        for hook in doctor.HOOKS:
            self.status(hook)

    def assert_clean(self, events):
        self.assertFalse([e for e in events if e[0] in ('FAIL','WARN')], events)

    def assert_event(self, events, level, text):
        self.assertTrue(any(l==level and text in message for l,message in events), events)

    def test_desktop_inactive_and_optional_plugin_absent_are_valid(self):
        self.assert_clean(self.diagnose())

    def test_gaming_attached_hooks_valid(self):
        self.attach()
        self.assert_clean(self.diagnose())

    def test_missing_payload_fails(self):
        (self.runtime/'steam-hooks/steam-client-update-hook/backend.mjs').unlink()
        self.assert_event(self.diagnose(), 'FAIL', 'backend.mjs')

    def test_corrupt_payload_fails_integrity(self):
        (self.runtime/'steam-hooks/steam-youtube-pause-hook/frontend.mjs').write_text('corruption')
        self.assert_event(self.diagnose(), 'FAIL', 'integridad')

    def test_node_old_missing_or_unusable_fails(self):
        for version in ('v20.0.0', None, 'not a version'):
            self.node = version
            self.assert_event(self.diagnose(), 'FAIL', 'Node.js')

    def test_no_user_bus_is_not_service_failure(self):
        self.bus = False
        self.assert_clean(self.diagnose())
        self.assert_event(self.diagnose(), 'INFO', 'no accesible')

    def test_gaming_hook_inactive_or_failed_fails(self):
        self.attach()
        for state in ('inactive','failed'):
            self.overrides['ugm-steam-client-update-hook.service'] = {'ActiveState':state}
            self.assert_event(self.diagnose(), 'FAIL', 'servicio no operativo')

    def test_active_hook_outside_ugm_warns(self):
        self.overrides['ugm-steam-client-update-hook.service'] = {'ActiveState':'active'}
        self.assert_event(self.diagnose(), 'WARN', 'fuera de sesión UGM')

    def test_local_unit_mask_dropin_and_global_enable_warn(self):
        path = self.home/'.config/systemd/user/ugm-steam-client-update-hook.service'
        path.parent.mkdir(parents=True)
        path.symlink_to('/dev/null')
        self.overrides[path.name] = {'LoadState':'masked','FragmentPath':'/dev/null', 'DropInPaths':'custom.conf', 'UnitFileState':'enabled'}
        events = self.diagnose()
        for level, text in (('WARN','antigua'), ('FAIL','enmascarada'), ('WARN','drop-ins'), ('WARN','habilitado')):
            self.assert_event(events, level, text)
        self.assertTrue(path.is_symlink())

    def test_stale_status_not_reported_attached(self):
        self.attach()
        self.status('steam-youtube-pause-hook', time=(self.now-datetime.timedelta(seconds=31)).isoformat())
        self.assert_event(self.diagnose(), 'WARN', 'estado histórico')

    def test_incompatible_or_waiting_hook_warns(self):
        self.attach()
        for state in ('waiting-or-incompatible','waiting-shared-context','ambiguous-shared-context','waiting-gaming-mode'):
            self.status('steam-youtube-pause-hook', state=state)
            self.assert_event(self.diagnose(), 'WARN', 'no confirmado')

    def test_invalid_hook_status_does_not_claim_healthy(self):
        self.attach()
        for data in ({}, {'active':False}):
            self.status('steam-youtube-pause-hook', hook=data)
            self.assert_event(self.diagnose(), 'WARN', 'sin confirmación')
        self.status('steam-client-update-hook', hook={'version':'1', 'installedBuild':0})
        self.assert_event(self.diagnose(), 'WARN', 'sin confirmación')

    def test_malformed_status_warns_and_continues(self):
        self.attach()
        path = self.home/'.local/state/ubuntu-gaming-mode/steam-youtube-pause-hook/status.json'
        for text in ('{', '[]', '{"time":null}', '{"time":"2026-11-03T18:00:00"}'):
            path.write_text(text)
            self.assert_event(self.diagnose(), 'WARN', 'ilegible')

    def test_updater_last_error_warns(self):
        self.attach()
        self.status('steam-client-update-hook', hook={'version':'2', 'installedBuild':1792618285, 'lastError':'failed'})
        self.assert_event(self.diagnose(), 'WARN', 'última operación')

    def test_historical_status_ignored_on_desktop(self):
        self.status('steam-youtube-pause-hook', state='waiting-or-incompatible')
        self.assert_clean(self.diagnose())

    def install_plugin(self):
        target = self.home/'homebrew/plugins/ugm-options'
        shutil.copytree(self.options, target)
        loader = self.home/'homebrew/services/PluginLoader'
        loader.parent.mkdir(parents=True)
        loader.write_text('must never execute')
        loader.chmod(0o755)
        return target

    def test_installed_plugin_complete_is_valid(self):
        self.install_plugin()
        self.assert_clean(self.diagnose())

    def test_incomplete_plugin_fails_and_duplicate_warns(self):
        target = self.install_plugin()
        (target/'modules/video/video_selector.py').unlink()
        (target.parent/'output-manager').mkdir()
        events = self.diagnose()
        self.assert_event(events, 'FAIL', 'video_selector.py')
        self.assert_event(events, 'WARN', 'doble controlador')

    def test_legacy_plugins_next_to_master_are_warned(self):
        target = self.install_plugin()
        for slug in doctor.LEGACY:
            (target.parent/slug).mkdir()
        events = self.diagnose()
        for slug in doctor.LEGACY:
            self.assert_event(events, 'WARN', slug)

    def test_custom_plugin_preserved_and_warned(self):
        target = self.install_plugin()
        (target/'main.py').write_text('raise AssertionError("never execute")')
        self.assert_event(self.diagnose(), 'WARN', 'copia existente conservada')
        self.assertIn('never execute', (target/'main.py').read_text())

    def test_missing_integrity_manifest_warns(self):
        self.md5.unlink()
        self.assert_event(self.diagnose(), 'WARN', 'integridad dpkg')

    def test_no_valid_account_skips_user_access(self):
        self.assert_event(self.diagnose(account=False), 'WARN', 'sin usuario/UID')
        self.assertEqual(self.calls, [['/usr/bin/node', '--version']])

    def test_unit_lifecycle_or_scope_tampering_fails(self):
        path = self.units/'gamescope-session-plus@.service'
        path.write_text('[Service]\nExecStart=/bin/true\n')
        hook = self.units/'ugm-steam-client-update-hook.service'
        hook.write_text(hook.read_text()+'\n[Install]\nWantedBy=default.target\n')
        self.assert_event(self.diagnose(), 'FAIL', 'ciclo')
        self.assert_event(self.diagnose(), 'FAIL', 'contrato de sesión')

    def test_diagnosis_does_not_change_any_file(self):
        self.attach()
        self.install_plugin()
        def snapshot():
            return {str(p):(p.read_bytes(),p.stat().st_mtime_ns,p.stat().st_mode) for p in self.base.rglob('*') if p.is_file()}
        before = snapshot()
        self.assert_clean(self.diagnose())
        self.assertEqual(before, snapshot())


class DoctorIntegrationTests(unittest.TestCase):
    def test_component_result_counts_and_crash_propagate_to_main_doctor(self):
        source = (ROOT/'src/ubuntu-gaming-mode-doctor').read_text()
        section = source.split('section "YOUTUBE, ACTUALIZADOR Y UGM OPTIONS"',1)[1].split('# ============================================================',1)[0]
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/'ugm-doctor-components'
            for report,code,expected in [('UGM_COMPONENT_COUNTS=2,3,1',1,'2,3,1'), ('UGM_COMPONENT_COUNTS=2,0,0',0,'2,0,0'), ('',1,'0,0,1'), ('UGM_COMPONENT_COUNTS=0,0,0',1,'0,0,1')]:
                path.write_text('#!/bin/sh\necho "'+report+'"\nexit '+str(code)+'\n')
                path.chmod(0o755)
                script = 'set -u\nOK=0; WARN=0; FAIL=0\nRUNTIME="'+temp+'"\nfail() { FAIL=$((FAIL+1)); }\n'+section+'\nprintf "%s,%s,%s\\n" "$OK" "$WARN" "$FAIL"\n'
                result = subprocess.run(['bash','-c',script],capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(result.stdout.strip().splitlines()[-1],expected)

    def test_packaging_copies_and_syntax_checks_python_helper(self):
        build = (ROOT/'packaging/build-deb.sh').read_text()
        self.assertIn('"$f" == */ugm-doctor-components', build)
        self.assertIn('"$WORK/usr/lib/ubuntu-gaming-mode/ugm-doctor-components" <<\'PYHELPERS\'', build)
