#!/bin/bash
set -euo pipefail

HOOK_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
NODE_BIN="$(command -v node)"
"$NODE_BIN" --input-type=module -e '
if (Number(process.versions.node.split(".")[0]) < 22 ||
    typeof WebSocket !== "function" || typeof fetch !== "function") {
  console.error("El hook requiere Node.js >= 22 con WebSocket y fetch globales.");
  process.exit(1);
}'
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
python3 - "$HOOK_DIR" "$NODE_BIN" "$UNIT_DIR" <<'PYUNIT'
from pathlib import Path
import os
import sys

hook_dir, node_bin, unit_dir = map(Path, sys.argv[1:])
def unit_arg(value):
    value = str(value)
    if any(ord(c) < 32 or ord(c) == 127 for c in value):
        raise SystemExit("Ruta con caracteres de control no admitida")
    value = value.replace('\\', '\\\\').replace('"', '\\"')
    return '"' + value.replace('%', '%%').replace('$', '$$') + '"'

text = (hook_dir / 'ugm-steam-youtube-pause-hook.service.in').read_text()
text = text.replace('@NODE@', unit_arg(node_bin))
text = text.replace('@SERVICE@', unit_arg(hook_dir / 'service.mjs'))
unit_dir.mkdir(parents=True, exist_ok=True)
unit = unit_dir / 'ugm-steam-youtube-pause-hook.service'
temporary = unit.with_suffix('.service.tmp')
temporary.write_text(text)
temporary.chmod(0o644)
os.replace(temporary, unit)
PYUNIT
systemctl --user daemon-reload
systemctl --user enable ugm-steam-youtube-pause-hook.service
systemctl --user restart ugm-steam-youtube-pause-hook.service
printf 'Servicio instalado desde %s. Parche YouTube habilitado; Steam no se ha reiniciado.\n' "$HOOK_DIR"
