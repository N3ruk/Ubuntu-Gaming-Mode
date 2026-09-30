# Ubuntu Gaming Mode source and reproducibility

**English** | [Español](SOURCE_es.md)

This repository contains **Ubuntu Gaming Mode 2.0.0-1** and the reproducible
record for its bundled Gamescope runtime.

## Layout

- `src/`: runtime scripts and session helpers.
- `system/`: systemd units, desktop/session entries and configuration.
- `packaging/DEBIAN/`: Debian metadata and maintainer scripts.
- `packaging/build-deb.sh`: reproducible package assembly.
- `gamescope/versions/3.16.30-gbm/`: patches, provenance and validation.
- `tests/`: tests for optional integrations.
- `docs/`: installation, rollback, architecture and performance.

## Binary inputs not stored in Git

The build requires:

1. the validated Gamescope executable;
2. the UGM PNG icon.

```bash
GAMESCOPE_BIN=/path/to/gamescope \
UGM_ICON=/path/to/ubuntu-gaming-mode.png \
bash packaging/build-deb.sh
```

The script rejects a Gamescope binary whose SHA256 is not authorized for the
package version. Both assets are included in the published `.deb`, not in Git.

## Gamescope 3.16.30 GBM

- Upstream base: `ad2763da1c48860f649abfe842a087188dcb6e20`.
- Patches 0001–0008: upstream-derived GBM port.
- Patch 0009: UGM safety fallback.
- Final tree: `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.
- Binary SHA256:
  `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.

Exact source preparation:

```bash
git clone https://github.com/ValveSoftware/gamescope.git
cd gamescope
git checkout ad2763da1c48860f649abfe842a087188dcb6e20
git submodule update --init --recursive
git am /path/to/gamescope/versions/3.16.30-gbm/patches/*.patch
```

See [`BUILD.md`](gamescope/versions/3.16.30-gbm/BUILD.md) for the toolchain and
[`VALIDATION.md`](gamescope/versions/3.16.30-gbm/VALIDATION.md) for test scope.

The embedded version remains `3.16.30-8-gb211c9d` because the ninth change was
built before its source diff became a numbered commit. Runtime identity is
anchored by the binary SHA256 and final source tree above.

## Historical 1.0.0-3 note

The modified Gamescope 3.16.28 tree used for release 1.0.0-3 was lost and cannot
be reconstructed. That historical limitation does not apply to 2.0.0-1: the
3.16.30 runtime and all nine patches are fully documented.
