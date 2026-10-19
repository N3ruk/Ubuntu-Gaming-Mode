#!/usr/bin/env bash
# Requires run.sh fixtures and a GPL-capable physical GPU. No game state edited.
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
route="${UGM_TEST_ROUTE:-nvidia}"
build="${1:-$root/.build/vrs-policy-tests}"
case "$route" in nvidia|amd) ;; *) exit 2;; esac
for gpl in 1 2; do
    UGM_TEST_GPL="$gpl" bash "$root/prototypes/$route-vrs/tests/run-gpu.sh" "$build"
    UGM_TEST_GPL="$gpl" UGM_VRS_AUDIT_ONLY=1 bash "$root/prototypes/$route-vrs/tests/run-gpu.sh" "$build"
done
