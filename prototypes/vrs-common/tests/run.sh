#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
build="${1:-$root/.build/vrs-policy-tests}"
mkdir -p "$build"
bash "$root/prototypes/nvidia-vrs/build.sh"
bash "$root/prototypes/amd-vrs/build.sh"
bash "$root/prototypes/nvidia-vrs/tests/run.sh" "$build"
for arch in 64 32; do
    cc -m"$arch" -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -O2 -g -Wall -Wextra -Werror "$root/prototypes/vrs-common/tests/test_rate.c" -lpthread -o "$build/rate-$arch"
    "$build/rate-$arch"
    for vendor in nvidia amd; do
        opts=(); [[ "$vendor" == amd ]] && opts=(-DUGM_TEST_AMD)
        cc -m"$arch" -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -O2 -g -Wall -Wextra -Werror "${opts[@]}" "$root/prototypes/vrs-common/tests/test_core.c" -lpthread -o "$build/core-$vendor-$arch"
        "$build/core-$vendor-$arch"
        if [[ "$vendor" == amd ]]; then
            cc -m"$arch" -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -O2 -g -Wall -Wextra -Werror "${opts[@]}" "$root/prototypes/nvidia-vrs/test_layer.c" -lpthread -o "$build/policy-amd-$arch"
            "$build/policy-amd-$arch" "$build/optimized"
            "$build/policy-amd-$arch" "$build/unoptimized"
        fi
    done
done
for test in rate core; do
    cc -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined "$root/prototypes/vrs-common/tests/test_$test.c" -lpthread -o "$build/$test-sanitized"
    "$build/$test-sanitized"
done
cc -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -DUGM_TEST_AMD "$root/prototypes/vrs-common/tests/test_core.c" -lpthread -o "$build/core-amd-sanitized"
"$build/core-amd-sanitized"
cc -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -DUGM_TEST_AMD "$root/prototypes/nvidia-vrs/test_layer.c" -lpthread -o "$build/policy-amd-sanitized"
"$build/policy-amd-sanitized" "$build/optimized"
"$build/policy-amd-sanitized" "$build/unoptimized"
printf '%s\n' 'Shared VRS test matrix: PASS (GPU tests are separate)'
