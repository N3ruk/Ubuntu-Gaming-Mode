"""Assemble a deterministic standalone master ZIP. Never install/build UGM DEB."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parent


def adapt_frontend(source, spec):
    """Only redirect Decky's connection and callable transport; retain panel body."""
    manifest = re.search(r'^const manifest = (.*);$', source, re.M)
    if not manifest or json.loads(manifest[1])['name'] != spec['name']:
        raise ValueError('Unexpected frontend manifest: ' + spec['id'])
    source = source[:manifest.start()] + 'const manifest = {"name":"UGM Options"};' + source[manifest.end():]
    bridge = 'const callable = api.callable;'
    if source.count(bridge) != 1 or source.count('export { index as default };') != 1:
        raise ValueError('Unsupported Decky frontend bridge: ' + spec['id'])
    methods = set(re.findall(r"callable\(['\"]([^'\"]+)", source))
    if not methods <= set(spec['methods']):
        raise ValueError('Frontend RPC not registered: ' + repr(methods - set(spec['methods'])))
    source = source.replace(bridge, 'const callable = method => (...args) => moduleCall(' + json.dumps(spec['id']) + ', method, args);')
    source = source.replace('export { index as default };', 'return index();')
    source = re.sub(r'^//# sourceMappingURL=.*$', '', source, flags=re.M)
    return source


def assemble(root=ROOT):
    specs = json.loads((root / 'modules.json').read_text())
    ids = [s['id'] for s in specs]
    if len(ids) != len(set(ids)) or any(not re.fullmatch('[a-z][a-z0-9]*', i) for i in ids):
        raise ValueError('Invalid/duplicate module ID')
    files = {}
    modules = {}
    factories = []
    for spec in specs:
        origin = root.parent / spec['slug']
        paths = list(spec['files'])
        if (origin / 'licenses').is_dir():
            paths += [str(p.relative_to(origin)) for p in (origin / 'licenses').rglob('*') if p.is_file()]
        hashes = {}
        for name in sorted(set(paths)):
            path = origin / name
            if not path.is_file() or path.is_symlink():
                raise ValueError('Missing/unsafe module file: ' + str(path))
            data = path.read_bytes()
            hashes[name] = hashlib.sha256(data).hexdigest()
            files['modules/' + spec['id'] + '/' + name] = data
        metadata = json.loads((origin / 'plugin.json').read_text())
        spec = dict(spec, name=metadata['name'])
        modules[spec['id']] = dict(slug=spec['slug'], name=metadata['name'],
            version=json.loads((origin / 'package.json').read_text())['version'], sha256=hashes)
        body = adapt_frontend((origin / 'dist/index.js').read_text(), spec)
        factories.append(json.dumps(spec['id']) + ': () => {\n' + body + '\n}')
    registry = [{'id': s['id'], 'label': s['label']} for s in specs]
    script = 'const moduleRegistry = ' + json.dumps(registry) + ';\n'
    script += 'const moduleFactories = {\n' + ',\n'.join(factories) + '\n};\n'
    script += (root / 'src/container.js').read_text()
    files['dist/index.js'] = script.encode()
    files['licenses/UGM-MIT.txt'] = (root.parents[2] / 'LICENSE').read_bytes()
    for name in ['main.py', 'backend.py', 'worker.py', 'modules.json', 'plugin.json',
                 'package.json', 'README.md', 'README_ES.md', 'NOTICE.md', 'LICENSE']:
        files[name] = (root / name).read_bytes()
    manifest = dict(schema=1, version=json.loads(files['package.json'])['version'],
                    status='packaged-for-ugm-2.8.5', modules=modules,
                    sha256={n: hashlib.sha256(d).hexdigest() for n, d in sorted(files.items())})
    files['manifest.json'] = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode()
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'release')
    args = parser.parse_args()
    files = assemble()
    # Staging is local to this new plugin, never in ~/homebrew or UGM's dist.
    for name, data in files.items():
        dest = ROOT / name
        if name.startswith(('modules/', 'dist/', 'licenses/')) or name == 'manifest.json':
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
            dest.chmod(0o755 if name.endswith('/ugm-active-display') else 0o644)
    args.out.mkdir(parents=True, exist_ok=True)
    target = args.out / 'UGM-Options-1.0.0-dev.zip'
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo('ugm-options/' + name, date_time=(2026, 11, 5, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (0o100755 if name.endswith('/ugm-active-display') else 0o100644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    print(json.dumps(dict(path=str(target), files=len(files), sha256=hashlib.sha256(target.read_bytes()).hexdigest())))


if __name__ == '__main__':
    main()
