#!/usr/bin/env bash
# Physical GPU proof is intentionally separate from cross-vendor mock coverage.
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
route="${UGM_TEST_ROUTE:-nvidia}"
case "$route" in nvidia|amd) ;; *) echo 'Expected nvidia or amd route' >&2; exit 2;; esac
build="${1:-$root/.build/vrs-policy-tests}"
unset UGM_TEST_EXTENDED_STATE UGM_VRS_AUDIT_ONLY
bash "$root/prototypes/$route-vrs/tests/run-gpu.sh" "$build"
UGM_TEST_EXTENDED_STATE=1 bash "$root/prototypes/$route-vrs/tests/run-gpu.sh" "$build"
UGM_TEST_EXTENDED_STATE=1 UGM_VRS_AUDIT_ONLY=1 bash "$root/prototypes/$route-vrs/tests/run-gpu.sh" "$build"
