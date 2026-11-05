import asyncio
import importlib.util
import json
import logging
import os
from pathlib import Path
import pwd
import shutil
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
loader = importlib.util.spec_from_file_location('options_backend', ROOT / 'backend.py')
backend = importlib.util.module_from_spec(loader)
loader.loader.exec_module(backend)
loader = importlib.util.spec_from_file_location('options_build', ROOT / 'build.py')
build = importlib.util.module_from_spec(loader)
loader.loader.exec_module(build)

ENGINE = '''import asyncio, json, os
from pathlib import Path
import decky
class Plugin:
    async def _main(self):
        self.path = Path(decky.DECKY_PLUGIN_SETTINGS_DIR)
        self.state = json.loads((self.path/'saved.json').read_text()) if (self.path/'saved.json').exists() else {'value':0}
    async def echo(self, *args):
        return {'args':args, 'uid':os.geteuid(), 'settings':str(self.path), 'state':self.state}
    async def fail(self):
        raise ValueError('expected failure')
    async def increment(self):
        v=self.state['value']; await asyncio.sleep(.01); self.state['value']=v+1
        return self.state['value']
    async def _unload(self):
        (self.path/'unloaded').write_text('yes')
'''


class WorkerTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        self.root = self.base/'plugins'/'ugm-options'
        self.root.mkdir(parents=True)
        self.settings = self.base/'settings'
        self.settings.mkdir()
        self.account = pwd.getpwuid(os.getuid())
        self.log = logging.getLogger('test-options')
        self.log.addHandler(logging.NullHandler())
        self.specs = [dict(id=i, label=i, slug='old-'+i, privileged=False,
                           methods=['echo','fail','increment']) for i in ['one','two']]
        (self.root/'modules.json').write_text(json.dumps(self.specs))
        shutil.copyfile(ROOT/'worker.py', self.root/'worker.py')
        for s in self.specs:
            d=self.root/'modules'/s['id'];d.mkdir(parents=True)
            (d/'main.py').write_text(ENGINE)
        self.workers = []

    def worker(self, spec=None):
        w=backend.Worker(self.root, spec or self.specs[0], self.settings, self.account, self.log)
        self.workers.append(w)
        return w

    async def asyncTearDown(self):
        for w in self.workers:
            await w.close()
        self.temp.cleanup()

    async def test_real_worker_uid_settings_and_unload(self):
        settings=self.settings/'old-one';settings.mkdir()
        (settings/'saved.json').write_text('{"value":37,"other":"preserved"}')
        w=self.worker();await w.start()
        self.assertTrue(w.status()['ready'],w.error)
        r=await w.call('echo',['a path with spaces',{'x':3}])
        self.assertEqual(r['uid'],os.getuid())
        self.assertNotEqual(r['uid'],0)
        self.assertEqual(r['settings'],str(settings))
        self.assertEqual(r['state'],{'value':37,'other':'preserved'})
        await w.close()
        self.assertEqual((settings/'unloaded').read_text(),'yes')
        self.assertEqual((settings/'saved.json').read_text(),'{"value":37,"other":"preserved"}')

    async def test_calls_are_serialized(self):
        w=self.worker();await w.start()
        values=await asyncio.gather(*(w.call('increment',[]) for _ in range(12)))
        self.assertEqual(values,list(range(1,13)))

    async def test_rpc_allowlist_rejects_internal_methods(self):
        w=self.worker();await w.start()
        for method,args in [('_unload',[]),('__dict__',[]),('echo',{})]:
            with self.assertRaises(ValueError):await w.call(method,args)
        self.assertTrue(w.status()['ready'])

    async def test_operation_error_does_not_kill_engine(self):
        w=self.worker();await w.start()
        with self.assertRaisesRegex(RuntimeError,'expected failure'):await w.call('fail',[])
        self.assertEqual((await w.call('echo',[]))['state']['value'],0)

    async def test_standalone_conflict_does_not_spawn(self):
        (self.root.parent/'old-one').mkdir()
        w=self.worker();await w.start()
        self.assertIsNone(w.process)
        self.assertIn('independiente',w.error)

    async def test_video_output_manager_conflict(self):
        (self.root.parent/'output-manager').mkdir()
        spec=dict(self.specs[0],id='video')
        w=self.worker(spec);await w.start()
        self.assertIn('output-manager',w.error)
        self.assertIsNone(w.process)

    async def test_one_worker_crash_does_not_affect_other(self):
        one=self.worker();two=self.worker(self.specs[1])
        await asyncio.gather(one.start(),two.start())
        one.process.kill();await one.process.wait()
        with self.assertRaises(RuntimeError):await one.call('echo',[])
        self.assertTrue(two.status()['ready'])
        self.assertEqual((await two.call('echo',[]))['uid'],os.getuid())

    async def test_failed_import_is_contained(self):
        (self.root/'modules/one/main.py').write_text('raise RuntimeError("broken engine")')
        w=self.worker();await w.start()
        self.assertFalse(w.status()['ready'])
        self.assertIsNotNone(w.process.returncode)

    async def test_root_video_cannot_start_unprivileged(self):
        w=self.worker(dict(self.specs[0],privileged=True))
        await w.start()
        self.assertFalse(w.status()['ready'])
        self.assertIn('privilegiado',w.error)

    async def test_root_parent_drops_credentials_before_import(self):
        w=self.worker()
        with patch.object(backend.os,'geteuid',return_value=0), \
             patch.object(backend.os,'chown'), \
             patch.object(backend.asyncio,'create_subprocess_exec',side_effect=RuntimeError('captured')) as spawn:
            await w.start()
        kwargs=spawn.call_args.kwargs
        self.assertEqual(kwargs['user'],self.account.pw_uid)
        self.assertEqual(kwargs['group'],self.account.pw_gid)
        self.assertEqual(kwargs['extra_groups'],os.getgrouplist(self.account.pw_name,self.account.pw_gid))
        self.assertEqual(spawn.call_args.args[:2],('/usr/bin/python3','-I'))

    async def test_broker_unknown_module_rejected(self):
        b=backend.Broker(self.root,self.settings,self.log)
        with self.assertRaises(ValueError):await b.call('../video','echo',[])

    async def test_real_nonsteam_worker_preserves_root_guard_and_reads_context(self):
        spec=next(s for s in json.loads((ROOT/'modules.json').read_text()) if s['id']=='nonsteam')
        d=self.root/'modules/nonsteam';shutil.copytree(ROOT/'modules/nonsteam',d)
        (self.root/'modules.json').write_text(json.dumps([spec]))
        w=self.worker(spec);await w.start()
        context=await w.call('get_context',[])
        self.assertTrue(context['ok'])
        self.assertEqual(context['data']['home'],self.account.pw_dir)
        self.assertEqual(context['data']['version'],'1.0.0')
        result=await w.call('list_applications',['',0])
        self.assertTrue(result['ok'])
        self.assertGreaterEqual(result['data']['total'],0)


class BuildTests(unittest.TestCase):
    def test_all_module_runtime_files_are_exact_copies(self):
        files=build.assemble()
        manifest=json.loads(files['manifest.json'])
        for spec in json.loads((ROOT/'modules.json').read_text()):
            for filename in manifest['modules'][spec['id']]['sha256']:
                self.assertEqual(files['modules/'+spec['id']+'/'+filename],
                                 (ROOT.parent/spec['slug']/filename).read_bytes())

    def test_unknown_frontend_bridge_rejected(self):
        spec=dict(id='test',name='Original',methods=[])
        with self.assertRaises(ValueError):build.adapt_frontend('const manifest = {"name":"wrong"};',spec)
        with self.assertRaises(ValueError):build.adapt_frontend('const manifest = {"name":"Original"};',spec)

    def test_build_determinism_and_no_private_paths(self):
        one=build.assemble();two=build.assemble()
        self.assertEqual(one,two)
        for name,data in one.items():
            self.assertNotIn(b'/home/n3ruk',data,name)
            self.assertNotIn('node_modules',Path(name).parts)
        self.assertEqual(json.loads(one['plugin.json'])['flags'],['root'])

    def test_root_settings_owner_rejected(self):
        with patch.object(Path,'stat') as stat:
            stat.return_value.st_uid=0
            with self.assertRaises(RuntimeError):backend.user_account(Path('/settings'))


if __name__=='__main__':unittest.main()
