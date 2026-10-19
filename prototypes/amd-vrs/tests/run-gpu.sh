#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
build="${1:-$root/../../.build/vrs-policy-tests}"
# Run tests/run.sh first to generate and validate the shader fixtures.
export VK_LAYER_PATH="$root${VK_LAYER_PATH:+:$VK_LAYER_PATH}"
export VK_INSTANCE_LAYERS="VK_LAYER_UGM_amd_vrs${VK_INSTANCE_LAYERS:+:$VK_INSTANCE_LAYERS}"
for arch in ${UGM_TEST_ARCHES:-64 32}; do
    cc -m"$arch" -std=c11 -O2 -Wall -Wextra -Werror "$root/tests/render.c" -l:libvulkan.so.1 -o "$build/render-$arch"
    for mode in optimized unoptimized; do
        "$build/render-$arch" "$build/$mode"
        UGM_TEST_APP_VRS=1 "$build/render-$arch" "$build/$mode"
        UGM_TEST_APP_VRS=2 "$build/render-$arch" "$build/$mode"
    done
done
