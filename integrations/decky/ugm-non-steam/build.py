"""Build an independent development ZIP. Never build/install UGM or restart Decky."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

FILES = ('main.py', 'catalog.py', 'shortcuts.py', 'plugin.json', 'package.json',
         'dist/index.js', 'LICENSE', 'NOTICE.md', 'ORIGIN.json', 'README.md', 'README_ES.md')


def main():
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=root / 'release')
    out = parser.parse_args().out
    plugin = json.loads((root / 'plugin.json').read_text())
    if plugin['flags']:
        raise ValueError('This plugin must not request root privileges')
    version = json.loads((root / 'package.json').read_text())['version']
    files = {}
    for name in FILES:
        path = root / name
        if not path.is_file() or path.is_symlink():
            raise ValueError('Missing/unsafe runtime file: ' + name)
        files[name] = path.read_bytes()
    manifest = dict(schema=1, version=version, status='development-unverified-in-UGM',
                    sha256={name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())})
    files['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    out.mkdir(parents=True, exist_ok=True)
    target = out / ('UGM-Non-Steam-' + version + '-dev.zip')
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo('ugm-non-steam/' + name, date_time=(2026, 11, 5, 0, 0, 0))
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    print(json.dumps(dict(path=str(target), sha256=hashlib.sha256(target.read_bytes()).hexdigest(), files=len(files))))


if __name__ == '__main__':
    main()
