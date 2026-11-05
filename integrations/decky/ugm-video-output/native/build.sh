#!/bin/bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
work="$(mktemp -d)"
trap 'python3 -c "import shutil,sys; shutil.rmtree(sys.argv[1])" "$work"' EXIT
wayland-scanner client-header "$root/native/gamescope-control.xml" "$work/gamescope-control-client-protocol.h"
wayland-scanner private-code "$root/native/gamescope-control.xml" "$work/gamescope-control-protocol.c"
mkdir -p "$root/dist"
cc -std=c11 -O2 -Wall -Wextra -Werror -Wl,-z,relro,-z,now -I"$work" \
    "$root/native/active-display.c" "$work/gamescope-control-protocol.c" \
    $(pkg-config --cflags --libs wayland-client) -o "$root/dist/ugm-active-display"
chmod 0755 "$root/dist/ugm-active-display"
