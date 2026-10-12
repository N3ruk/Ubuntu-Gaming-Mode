#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
build="${1:-$root/../../.build/vrs-policy-tests}"
# Run tests/run.sh first to generate and validate the shader fixtures.
export VK_LAYER_PATH="$root"
export VK_INSTANCE_LAYERS=VK_LAYER_UGM_nvidia_vrs
for arch in 64 32; do
    cc -m"$arch" -std=c11 -O2 -Wall -Wextra -Werror "$root/tests/render.c" -l:libvulkan.so.1 -o "$build/render-$arch"
    for mode in optimized unoptimized; do
        "$build/render-$arch" "$build/$mode"
        UGM_TEST_APP_VRS=1 "$build/render-$arch" "$build/$mode"
    done
done
