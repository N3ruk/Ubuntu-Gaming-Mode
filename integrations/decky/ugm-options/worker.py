"""Private stdio worker. Load one unchanged engine after OS credential drop."""
import asyncio
import importlib.util
import json
import logging
import os
from pathlib import Path
import sys
import types


def send(value):
    sys.stdout.write(json.dumps(value, ensure_ascii=False) + '\n')
    sys.stdout.flush()


async def serve(root, spec, context):
    if os.geteuid() == 0 and not spec['privileged']:
        raise RuntimeError('Unprivileged engine must never run as root')
    if os.geteuid() != context['uid']:
        raise RuntimeError('Unexpected worker credentials')
    module_dir = root / 'modules' / spec['id']
    logger = logging.getLogger('ugm-options.' + spec['id'])
    logger.addHandler(logging.StreamHandler(sys.stderr))
    logger.setLevel(logging.INFO)
    # Each worker has its own Decky context, globals, imports and asyncio tasks.
    shim = types.ModuleType('decky')
    shim.logger = logger
    for key, value in context['paths'].items():
        setattr(shim, key, value)
    sys.modules['decky'] = sys.modules['decky_plugin'] = shim
    sys.path.insert(0, str(module_dir))
    module_spec = importlib.util.spec_from_file_location('ugm_engine', module_dir / 'main.py')
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    plugin = module.Plugin()
    try:
        await plugin._main()
        send({'ready': True, 'uid': os.geteuid(), 'module': spec['id']})
        while True:
            line = await asyncio.to_thread(sys.stdin.readline)
            if not line:
                break
            request = json.loads(line)
            if request == {'shutdown': True}:
                break
            try:
                method, args = request['method'], request['args']
                if method not in spec['methods'] or not isinstance(args, list):
                    raise ValueError('Unsupported module method')
                result = await getattr(plugin, method)(*args)
                send({'id': request['id'], 'ok': True, 'result': result})
            except Exception as error:
                logger.exception('Module request failed')
                send({'id': request.get('id'), 'ok': False, 'error': str(error)})
    finally:
        await plugin._unload()


def main():
    root = Path(__file__).resolve().parent
    module_id = sys.argv[1]
    spec = next(m for m in json.loads((root / 'modules.json').read_text()) if m['id'] == module_id)
    context = json.loads(sys.argv[2])
    asyncio.run(serve(root, spec, context))


if __name__ == '__main__':
    main()
