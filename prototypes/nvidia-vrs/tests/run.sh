#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
build="${1:-$root/../../.build/vrs-policy-tests}"
mkdir -p "$build/optimized" "$build/unoptimized"
for mode in optimized unoptimized; do
    opts=(); [[ "$mode" == optimized ]] && opts=(-Os)
    for file in "$root"/tests/shaders/*; do
        glslangValidator -V "${opts[@]}" "$file" -o "$build/$mode/$(basename "$file").spv" > "$build/compile.log" 2>&1 || { cat "$build/compile.log"; exit 1; }
        spirv-val --target-env vulkan1.0 "$build/$mode/$(basename "$file").spv"
    done
    python3 "$root/../vrs-common/tests/make-modern-fixtures.py" "$build/$mode"
    for arch in 64 32; do
        cc -m"$arch" -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -O2 -g -Wall -Wextra -Werror "$root/test_layer.c" -lpthread -o "$build/policy-$arch"
        "$build/policy-$arch" "$build/$mode"
    done
done
cc -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined "$root/test_layer.c" -lpthread -o "$build/policy-sanitized"
"$build/policy-sanitized" "$build/optimized"
"$build/policy-sanitized" "$build/unoptimized"
