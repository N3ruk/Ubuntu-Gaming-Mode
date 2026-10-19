#!/usr/bin/env bash
set -Eeuo pipefail
route="$1"
root="$2"
case "$route" in nvidia|amd) ;; *) exit 2;; esac
output="${UGM_VRS_OUTPUT_DIR:-$root}"
mkdir -p "$output"
output="$(cd -- "$output" && pwd -P)"
build="$(mktemp -d "$output/.ugm-build.XXXXXXXX")"
trap 'python3 -c "import shutil,sys; shutil.rmtree(sys.argv[1])" "$build"' EXIT
common=(-D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -O2 -fPIC -shared -Wall -Wextra -Werror -Wl,-Bsymbolic-functions)
for arch in 64 32; do
    "${CC:-cc}" -m"$arch" "${common[@]}" -o "$build/libugm_${route}_vrs_${arch}.so" "$root/layer.c" -lpthread
done
python3 - "$build" "${UGM_VRS_LIBRARY_DIR:-$output}" "$output" "$route" <<'PY'
import json
import os
from pathlib import Path
import sys
build, library_dir, output = map(Path, sys.argv[1:4])
route = sys.argv[4]
for arch in ("64", "32"):
    manifest = {"file_format_version": "1.2.1", "layer": {
        "name": f"VK_LAYER_UGM_{route}_vrs", "type": "GLOBAL",
        "library_path": str(library_dir / f"libugm_{route}_vrs_{arch}.so"),
        "library_arch": arch, "api_version": "1.3.0",
        "implementation_version": "1",
        "description": f"UGM experimental {route.upper()} pipeline VRS"}}
    (build / f"VK_LAYER_UGM_{route}_vrs_{arch}.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
# Linker never truncates an installed/mapped inode. Both builds must succeed
# before publication; library names and per-architecture manifests unchanged.
for path in sorted(build.iterdir(), key=lambda p: p.suffix != ".so"):
    os.replace(path, output / path.name)
PY
