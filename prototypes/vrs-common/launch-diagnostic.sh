#!/bin/sh
# Explicit, isolated runtime for either vendor. Never installs a system layer.
set -eu
if [ "$#" -lt 3 ]; then
    echo 'Usage: launch-diagnostic.sh nvidia|amd PAYLOAD COMMAND [ARGS...]' >&2
    exit 2
fi
route=$1
payload=$2
shift 2
case "$route" in nvidia|amd) ;; *) echo 'Unsupported VRS route' >&2; exit 2;; esac
for arch in 64 32; do
    if [ ! -f "$payload/VK_LAYER_UGM_${route}_vrs_${arch}.json" ] || [ ! -f "$payload/libugm_${route}_vrs_${arch}.so" ]; then
        echo 'UGM VRS: incomplete diagnostic payload' >&2
        exit 2
    fi
done
if [ -z "${UGM_VRS_CONFIG_FILE:-}" ] || [ ! -f "$UGM_VRS_CONFIG_FILE" ]; then
    echo 'UGM VRS: this launcher requires the session QAM configuration file' >&2
    exit 2
fi
# Steam imports explicit paths into its container. Do not import the installed
# manifest with the same layer name, or loader ordering can select the old DLL.
if [ -n "${VK_LAYER_PATH:-}" ]; then
    export VK_LAYER_PATH="$payload"
    unset VK_ADD_LAYER_PATH
else
    export VK_ADD_LAYER_PATH="$payload"
fi
layer="VK_LAYER_UGM_${route}_vrs"
case ":${VK_INSTANCE_LAYERS:-}:" in
    *:"$layer":*) ;;
    *) export VK_INSTANCE_LAYERS="$layer${VK_INSTANCE_LAYERS:+:$VK_INSTANCE_LAYERS}" ;;
esac
export UGM_VRS_DEBUG=1
export PROTON_LOG=1
exec "$@"
