#!/bin/bash
set -Eeuo pipefail
umask 022

# Fecha canónica de la release 2.0.0-1. dpkg-deb normaliza con ella los
# metadatos del archivo para que dos builds del mismo árbol sean idénticas.
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-1790726400}"

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
VERSION="$(sed -n 's/^Version:[[:space:]]*//p' "$ROOT/packaging/DEBIAN/control" | head -n1)"
ARCH="$(sed -n 's/^Architecture:[[:space:]]*//p' "$ROOT/packaging/DEBIAN/control" | head -n1)"

GAMESCOPE_BIN="${GAMESCOPE_BIN:-}"
UGM_ICON="${UGM_ICON:-}"

if [[ -z "$GAMESCOPE_BIN" || ! -f "$GAMESCOPE_BIN" ]]; then
    echo "ERROR: define GAMESCOPE_BIN con la ruta al binario Gamescope que se incluirá." >&2
    exit 1
fi

if [[ -z "$UGM_ICON" || ! -f "$UGM_ICON" ]]; then
    echo "ERROR: define UGM_ICON con la ruta al icono PNG de UGM." >&2
    exit 1
fi

OUTDIR="${OUTDIR:-$ROOT/dist}"
WORK="${WORKDIR:-$ROOT/.build/ubuntu-gaming-mode_${VERSION}_${ARCH}}"
OUT="$OUTDIR/ubuntu-gaming-mode_${VERSION}_${ARCH}.deb"

rm -rf "$WORK"
mkdir -p     "$WORK/DEBIAN"     "$WORK/etc/gamescope-session-plus/sessions.d"     "$WORK/usr/bin"     "$WORK/usr/lib/ubuntu-gaming-mode"     "$WORK/usr/lib/systemd/system"     "$WORK/usr/lib/systemd/user"     "$WORK/usr/share/applications"     "$WORK/usr/share/wayland-sessions"     "$WORK/usr/share/icons/hicolor/256x256/apps"     "$WORK/usr/share/doc/ubuntu-gaming-mode/licenses"     "$OUTDIR"

cp -a "$ROOT/packaging/DEBIAN/." "$WORK/DEBIAN/"

install -m 0755 "$ROOT/src/gamescope-session-plus"     "$WORK/usr/lib/ubuntu-gaming-mode/gamescope-session-plus"
install -m 0755 "$ROOT/src/gamescope-session-plus-runtime"     "$WORK/usr/lib/ubuntu-gaming-mode/gamescope-session-plus-runtime"
install -m 0755 "$ROOT/src/gaming-autologin-rearm"     "$WORK/usr/lib/ubuntu-gaming-mode/gaming-autologin-rearm"
install -m 0755 "$ROOT/src/os-session-select"     "$WORK/usr/lib/ubuntu-gaming-mode/os-session-select"
install -m 0755 "$ROOT/src/return-to-gaming-mode"     "$WORK/usr/lib/ubuntu-gaming-mode/return-to-gaming-mode"
install -m 0755 "$ROOT/src/steamos-session-select"     "$WORK/usr/lib/ubuntu-gaming-mode/steamos-session-select"
install -m 0755 "$ROOT/src/ubuntu-gaming-mode-doctor"     "$WORK/usr/lib/ubuntu-gaming-mode/ubuntu-gaming-mode-doctor"
install -m 0755 "$ROOT/src/ubuntu-gaming-mode-setup"     "$WORK/usr/lib/ubuntu-gaming-mode/ubuntu-gaming-mode-setup"
install -m 0755 "$ROOT/src/ugm-gaming-session-marker"     "$WORK/usr/lib/ubuntu-gaming-mode/ugm-gaming-session-marker"
install -m 0755 "$ROOT/src/ugm-powerbuttond"     "$WORK/usr/lib/ubuntu-gaming-mode/ugm-powerbuttond"
install -m 0755 "$ROOT/src/ugm-steam-shortpowerpress"     "$WORK/usr/lib/ubuntu-gaming-mode/ugm-steam-shortpowerpress"
install -m 0755 "$ROOT/src/ugm-system-gamescope-link"     "$WORK/usr/lib/ubuntu-gaming-mode/ugm-system-gamescope-link"

install -m 0755 "$GAMESCOPE_BIN"     "$WORK/usr/lib/ubuntu-gaming-mode/gamescope"

install -m 0644 "$ROOT/system/gamescope-session/steam"     "$WORK/etc/gamescope-session-plus/sessions.d/steam"
install -m 0644 "$ROOT/system/systemd/gaming-autologin-rearm.service"     "$WORK/usr/lib/systemd/system/gaming-autologin-rearm.service"
install -m 0644 "$ROOT/system/systemd/ugm-powerbuttond.service"     "$WORK/usr/lib/systemd/system/ugm-powerbuttond.service"
install -m 0644 "$ROOT/system/systemd/ugm-steam-shortpowerpress.service"     "$WORK/usr/lib/systemd/user/ugm-steam-shortpowerpress.service"
install -m 0644 "$ROOT/system/systemd/gamescope-session-plus@.service"     "$WORK/usr/lib/systemd/user/gamescope-session-plus@.service"
install -m 0644 "$ROOT/system/applications/ubuntu-gaming-mode.desktop"     "$WORK/usr/share/applications/ubuntu-gaming-mode.desktop"
install -m 0644 "$ROOT/system/wayland-sessions/steam-gaming-mode.desktop"     "$WORK/usr/share/wayland-sessions/steam-gaming-mode.desktop"
install -m 0644 "$UGM_ICON"     "$WORK/usr/share/icons/hicolor/256x256/apps/ubuntu-gaming-mode.png"

install -m 0644 "$ROOT/README.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/README.md"
install -m 0644 "$ROOT/README_es.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/README_es.md"
install -m 0644 "$ROOT/CHANGELOG.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/CHANGELOG.md"
install -m 0644 "$ROOT/CHANGELOG_es.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/CHANGELOG_es.md"
install -m 0644 "$ROOT/SOURCE.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/SOURCE.md"
install -m 0644 "$ROOT/SOURCE_es.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/SOURCE_es.md"
install -m 0644 "$ROOT/THIRD_PARTY_NOTICES.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/THIRD_PARTY_NOTICES.md"
install -m 0644 "$ROOT/THIRD_PARTY_NOTICES_es.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/THIRD_PARTY_NOTICES_es.md"
install -m 0644 "$ROOT/docs/package-manifest-2.0.0-1.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/package-manifest-2.0.0-1.md"
install -m 0644 "$ROOT/docs/es/manifiesto-2.0.0-1.md"     "$WORK/usr/share/doc/ubuntu-gaming-mode/manifiesto-2.0.0-1.md"
install -m 0644 "$ROOT/packaging/copyright"     "$WORK/usr/share/doc/ubuntu-gaming-mode/copyright"
install -m 0644 "$ROOT/licenses/CHIMERAOS-GAMESCOPE-SESSION-LICENSE"     "$WORK/usr/share/doc/ubuntu-gaming-mode/licenses/CHIMERAOS-GAMESCOPE-SESSION-LICENSE"
install -m 0644 "$ROOT/licenses/GAMESCOPE-LICENSE"     "$WORK/usr/share/doc/ubuntu-gaming-mode/licenses/GAMESCOPE-LICENSE"
install -m 0644 "$ROOT/packaging/changelog.Debian"     "$WORK/usr/share/doc/ubuntu-gaming-mode/changelog.Debian"
gzip -9n "$WORK/usr/share/doc/ubuntu-gaming-mode/changelog.Debian"

ln -s ../lib/ubuntu-gaming-mode/ubuntu-gaming-mode-setup     "$WORK/usr/bin/ubuntu-gaming-mode-setup"
ln -s ../lib/ubuntu-gaming-mode/ubuntu-gaming-mode-doctor     "$WORK/usr/bin/ubuntu-gaming-mode-doctor"
ln -s ../lib/ubuntu-gaming-mode/steamos-session-select     "$WORK/usr/bin/steamos-session-select"

for f in "$WORK/DEBIAN/config" "$WORK/DEBIAN/preinst" "$WORK/DEBIAN/postinst" "$WORK/DEBIAN/prerm" "$WORK/DEBIAN/postrm"; do
    chmod 0755 "$f"
    bash -n "$f"
done

for f in "$WORK/usr/lib/ubuntu-gaming-mode/"*; do
    [[ "$f" == */gamescope || "$f" == */ugm-powerbuttond || "$f" == */ugm-steam-shortpowerpress ]] && continue
    bash -n "$f"
done

python3 - "$WORK/usr/lib/ubuntu-gaming-mode/ugm-powerbuttond" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
compile(source, sys.argv[1], "exec")
PY

python3 - "$WORK/usr/lib/ubuntu-gaming-mode/ugm-steam-shortpowerpress" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
compile(source, sys.argv[1], "exec")
PY

EXPECTED_GAMESCOPE_SHA_1_0_0_3="e43f0737287b2812d0c34a43c638131b3656058e1666d55228ad8acfe59d616c"
EXPECTED_GAMESCOPE_SHA_2_0_0_1="5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256"
ACTUAL_GAMESCOPE_SHA="$(sha256sum "$WORK/usr/lib/ubuntu-gaming-mode/gamescope" | awk '{print $1}')"

case "$VERSION" in
    1.0.0-3)
        EXPECTED_GAMESCOPE_SHA="$EXPECTED_GAMESCOPE_SHA_1_0_0_3"
        ;;
    2.0.0-1)
        EXPECTED_GAMESCOPE_SHA="$EXPECTED_GAMESCOPE_SHA_2_0_0_1"
        ;;
    *)
        echo "ERROR: no hay un SHA256 Gamescope autorizado para la versión $VERSION" >&2
        exit 1
        ;;
esac

if [[ "$ACTUAL_GAMESCOPE_SHA" != "$EXPECTED_GAMESCOPE_SHA" ]]; then
    echo "ERROR: para $VERSION el Gamescope debe tener SHA256 $EXPECTED_GAMESCOPE_SHA" >&2
    echo "Actual: $ACTUAL_GAMESCOPE_SHA" >&2
    exit 1
fi

INSTALLED_SIZE="$(du -sk --exclude=DEBIAN "$WORK" | awk '{print $1}')"
sed -i "/^Installed-Size:/d" "$WORK/DEBIAN/control"
sed -i "/^Architecture:/a Installed-Size: $INSTALLED_SIZE" "$WORK/DEBIAN/control"

(
    cd "$WORK"
    find . -type f ! -path './DEBIAN/*' -print0 |
        sort -z |
        xargs -0 md5sum |
        sed 's#  \./#  #'
) > "$WORK/DEBIAN/md5sums"
chmod 0644 "$WORK/DEBIAN/md5sums"

dpkg-deb --build --root-owner-group "$WORK" "$OUT"

echo
echo "Paquete generado:"
echo "  $OUT"
sha256sum "$OUT"

if [[ "$VERSION" == "1.0.0-3" ]]; then
    echo
    echo "NOTA: el GOLD publicado 1.0.0-3 tiene SHA256:"
    echo "959afa06fd87eeeba8ffc1d4a3a6c965f9fa902d5678d43597b2ce7b20dca797"
    echo "La reproducibilidad byte-a-byte todavía no se garantiza: el árbol fuente fue"
    echo "publicado después de validar el artefacto GOLD y la receta exacta de Gamescope"
    echo "aún está pendiente de documentar."
fi
