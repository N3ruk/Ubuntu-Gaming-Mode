#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
output="${UGM_VRS_OUTPUT_DIR:-$root}"
mkdir -p "$output"
common=(-std=c11 -O2 -fPIC -shared -Wall -Wextra -Werror -Wl,-Bsymbolic-functions)
cc "${common[@]}" -o "$output/libugm_nvidia_vrs_64.so" "$root/layer.c" -lpthread
cc -m32 "${common[@]}" -o "$output/libugm_nvidia_vrs_32.so" "$root/layer.c" -lpthread
python3 - "$output" "${UGM_VRS_LIBRARY_DIR:-$output}" <<'PY'
import json
from pathlib import Path
import sys
root = Path(sys.argv[1])
library_dir = Path(sys.argv[2])
for arch in ("64", "32"):
    manifest = {"file_format_version": "1.2.1", "layer": {
        "name": "VK_LAYER_UGM_nvidia_vrs", "type": "GLOBAL",
        "library_path": str(library_dir / f"libugm_nvidia_vrs_{arch}.so"),
        "library_arch": arch, "api_version": "1.3.0",
        "implementation_version": "1",
        "description": "UGM experimental NVIDIA pipeline VRS"}}
    (root / f"VK_LAYER_UGM_nvidia_vrs_{arch}.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
PY
