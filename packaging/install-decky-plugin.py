#!/usr/bin/python3
"""Stage the verified UGM Options runtime only; never build or install a DEB."""
import os
from pathlib import Path
import runpy
import shutil
import sys


def stage(root, destination):
    helper = runpy.run_path(str(root / 'src/ugm-decky-integration'))
    files_fn = helper['payload_files']
    files_fn.__globals__['PAYLOAD'] = root / 'integrations/decky'
    files = files_fn('ugm-options')
    target = destination / 'usr/share/ubuntu-gaming-mode/decky-plugins/ugm-options'
    for entry in files:
        path = target / entry
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / 'integrations/decky/ugm-options' / entry, path)
        path.chmod(0o755 if entry in helper['EXECUTABLES'] else 0o644)
        if path.suffix == '.py':
            compile(path.read_text(), str(path), 'exec')
    for directory, _, _ in os.walk(target):
        Path(directory).chmod(0o755)
    return target


if __name__ == '__main__':
    stage(Path(sys.argv[1]), Path(sys.argv[2]))
