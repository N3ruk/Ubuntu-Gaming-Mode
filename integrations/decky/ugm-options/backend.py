"""Fixed module broker: no shell, arbitrary imports or privilege-changing RPC."""
import asyncio
import json
import os
from pathlib import Path
import pwd

MAX_MESSAGE = 4 * 1024 * 1024


def user_account(settings_root):
    """Use Decky's settings owner, never a UID supplied by the frontend."""
    uid = settings_root.stat().st_uid
    if uid == 0:
        raise RuntimeError('Cannot determine a non-root Decky user from settings owner')
    return pwd.getpwuid(uid)


class Worker:
    def __init__(self, root, spec, settings_root, account, logger):
        self.root, self.spec, self.settings_root = root, spec, settings_root
        self.account, self.logger = account, logger
        self.process = None
        self.lock = asyncio.Lock()
        self.error = ''
        self.uid = None
        self.sequence = 0
        self.stopping = False

    def status(self):
        running = self.process is not None and self.process.returncode is None and self.uid is not None
        return {'id': self.spec['id'], 'label': self.spec['label'],
                'ready': running and not self.error and not self.stopping,
                'uid': self.uid, 'error': self.error or ('' if running else 'Módulo no iniciado')}

    async def start(self):
        legacy = self.root.parent / self.spec['slug']
        conflicts = [legacy] if legacy.exists() else []
        if self.spec['id'] == 'video':
            conflicts += [p for p in (self.root.parent / 'output-manager',) if p.exists()]
        if conflicts:
            self.error = 'Retira primero el plugin independiente: ' + ', '.join(p.name for p in conflicts)
            return
        uid = 0 if self.spec['privileged'] else self.account.pw_uid
        if self.spec['privileged'] and os.geteuid() != 0:
            self.error = 'Video Output necesita el broker privilegiado de Decky'
            return
        if os.geteuid() not in (0, uid):
            self.error = 'El worker no puede ejecutarse con el usuario de Decky'
            return
        try:
            settings = self.settings_root / self.spec['slug']
            if not settings.exists():
                settings.mkdir(mode=0o755)
                if os.geteuid() == 0:
                    os.chown(settings, uid, 0 if uid == 0 else self.account.pw_gid)
            context = {'uid': uid, 'paths': {
                'DECKY_PLUGIN_DIR': str(self.root / 'modules' / self.spec['id']),
                # Reuse original settings paths without resetting/converting them.
                'DECKY_PLUGIN_SETTINGS_DIR': str(settings),
                'DECKY_USER': self.account.pw_name,
                'DECKY_USER_HOME': self.account.pw_dir,
            }}
            env = dict(os.environ, HOME=self.account.pw_dir if uid else '/root',
                       USER=self.account.pw_name if uid else 'root',
                       LOGNAME=self.account.pw_name if uid else 'root',
                       PATH='/usr/bin:/bin', PYTHONUNBUFFERED='1')
            credentials = {}
            if os.geteuid() == 0 and uid:
                credentials = dict(user=uid, group=self.account.pw_gid,
                                   extra_groups=os.getgrouplist(self.account.pw_name, self.account.pw_gid))
            self.process = await asyncio.create_subprocess_exec(
                '/usr/bin/python3', '-I', str(self.root / 'worker.py'), self.spec['id'],
                json.dumps(context), stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE, stderr=None, limit=MAX_MESSAGE,
                env=env, cwd=str(self.root), **credentials)
            ready = json.loads(await asyncio.wait_for(self.process.stdout.readline(), 15))
            if ready != {'ready': True, 'uid': uid, 'module': self.spec['id']}:
                raise RuntimeError('Invalid worker startup handshake')
            self.uid = uid
        except Exception as error:
            self.error = str(error)
            self.logger.exception('UGM Options module startup: %s', self.spec['id'])
            await self.close()

    async def call(self, method, args):
        if method not in self.spec['methods'] or not isinstance(args, list):
            raise ValueError('Unsupported module method')
        async with self.lock:
            if not self.status()['ready']:
                raise RuntimeError(self.status()['error'])
            self.sequence += 1
            request = json.dumps({'id': self.sequence, 'method': method, 'args': args}).encode() + b'\n'
            if len(request) > MAX_MESSAGE:
                raise ValueError('Module request too large')
            try:
                self.process.stdin.write(request)
                await self.process.stdin.drain()
                line = await asyncio.wait_for(self.process.stdout.readline(), 90)
                result = json.loads(line)
                if result.get('id') != self.sequence:
                    raise RuntimeError('Invalid worker response')
            except BaseException as error:
                # Never replay a timed-out/cancelled write automatically.
                self.error = 'Comunicación interrumpida; recarga UGM Options. ' + str(error)
                await self._close()
                raise
            if not result.get('ok'):
                raise RuntimeError(result.get('error', 'Module operation failed'))
            return result['result']

    async def _close(self):
        if not self.process:
            return
        if self.process.returncode is None:
            try:
                self.process.stdin.write(b'{"shutdown":true}\n')
                await self.process.stdin.drain()
                await asyncio.wait_for(self.process.wait(), 10)
            except (TimeoutError, BrokenPipeError, ConnectionResetError):
                if self.process.returncode is None:
                    self.process.kill()
                await self.process.wait()
        self.uid = None

    async def close(self):
        self.stopping = True
        async with self.lock:
            await self._close()


class Broker:
    def __init__(self, root, settings_root, logger):
        self.root, self.settings_root, self.logger = root, settings_root, logger
        specs = json.loads((root / 'modules.json').read_text())
        self.specs = {m['id']: m for m in specs}
        self.workers = {}
        self.error = ''

    async def start(self):
        try:
            account = user_account(self.settings_root)
            self.workers = {key: Worker(self.root, spec, self.settings_root, account, self.logger)
                            for key, spec in self.specs.items()}
            await asyncio.gather(*(worker.start() for worker in self.workers.values()))
        except Exception as error:
            self.error = str(error)
            self.logger.exception('UGM Options startup')

    def status(self):
        return {key: self.workers[key].status() if key in self.workers else
                {'id': key, 'label': spec['label'], 'ready': False, 'uid': None,
                 'error': self.error or 'Módulo no iniciado'} for key, spec in self.specs.items()}

    async def call(self, module, method, args):
        if not isinstance(module, str) or module not in self.workers:
            raise ValueError('Unknown UGM module')
        if not isinstance(method, str):
            raise ValueError('Invalid module method')
        return await self.workers[module].call(method, args)

    async def close(self):
        await asyncio.gather(*(worker.close() for worker in self.workers.values()))
