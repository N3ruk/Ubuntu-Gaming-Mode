#!/bin/bash
# Stage only: no DEB, no systemctl and no changes to a real user HOME.
set -Eeuo pipefail
ROOT="${1:?source root required}"
DEST="${2:?staging root required}"
install -d -m 0755 "$DEST/usr/lib/ubuntu-gaming-mode/steam-hooks" "$DEST/usr/lib/systemd/user"
install -m 0755 "$ROOT/src/ugm-steam-hooks" "$DEST/usr/lib/ubuntu-gaming-mode/ugm-steam-hooks"
install -m 0755 "$ROOT/src/ugm-doctor-components" "$DEST/usr/lib/ubuntu-gaming-mode/ugm-doctor-components"
for hook in steam-youtube-pause-hook steam-client-update-hook; do
    source_dir="$ROOT/tools/$hook"
    target_dir="$DEST/usr/lib/ubuntu-gaming-mode/steam-hooks/$hook"
    install -d -m 0755 "$target_dir"
    for file in "$source_dir/"*.mjs; do
        [[ "$file" == *.test.mjs ]] && continue
        install -m 0644 "$file" "$target_dir/$(basename "$file")"
    done
    for file in "$source_dir/"*.json "$source_dir/"*.service.in "$source_dir/README.md"; do
        [[ -f "$file" ]] || continue
        install -m 0644 "$file" "$target_dir/$(basename "$file")"
    done
    install -m 0644 "$ROOT/system/systemd/ugm-$hook.service" "$DEST/usr/lib/systemd/user/ugm-$hook.service"
done

install -d -m 0755 "$DEST/usr/share/doc/ubuntu-gaming-mode/docs/es/ugm-2.8.5"
install -m 0644 "$ROOT/docs/ugm-2.8.5-preparation.md" "$DEST/usr/share/doc/ubuntu-gaming-mode/docs/ugm-2.8.5-preparation.md"
install -m 0644 "$ROOT/docs/es/ugm-2.8.5/PREPARACION-BUILD.md" "$DEST/usr/share/doc/ubuntu-gaming-mode/docs/es/ugm-2.8.5/PREPARACION-BUILD.md"

install -d -m 0755 "$DEST/usr/share/doc/ubuntu-gaming-mode/docs/ugm-2.8.5"
for file in README.md VERIFICATION.md; do
    install -m 0644 "$ROOT/docs/ugm-2.8.5/$file" "$DEST/usr/share/doc/ubuntu-gaming-mode/docs/ugm-2.8.5/$file"
done
install -m 0644 "$ROOT/docs/es/ugm-2.8.5/README.md" "$DEST/usr/share/doc/ubuntu-gaming-mode/docs/es/ugm-2.8.5/README.md"
