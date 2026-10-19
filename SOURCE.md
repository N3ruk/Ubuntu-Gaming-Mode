# Ubuntu Gaming Mode source and reproducibility

**English** | [Español](SOURCE_es.md)

This tree contains the **Ubuntu Gaming Mode 2.8.0-2** sources. It retains the historical
2.5 Gamescope 3.16.30 record and adds patch 0010 for 2.8.

## Layout

- `src/`: runtime scripts and session helpers.
- `system/`: systemd units, desktop/session entries and configuration.
- `packaging/DEBIAN/`: Debian metadata and maintainer scripts.
- `packaging/build-deb.sh`: reproducible package assembly.
- `gamescope/versions/3.16.30-gbm/`: patches, provenance and validation.
- `tests/`: tests for optional integrations.
- `docs/`: installation, rollback, architecture and performance.
- `prototypes/vrs-common/`, `prototypes/nvidia-vrs/` and
  `prototypes/amd-vrs/`: source, builds and tests for bundled VRS layers.
  Generated `.so` files are not tracked.
- `integrations/decky/`: minimal payload for the two optional plugins.
- `docs/ugm-2.8/` and `docs/es/ugm-2.8/`: bilingual feature documentation and limitations.
- `system/icons/`: tracked PNG icons for UGM and the Bluetooth app.

## Binary input and application icon

The build requires the validated Gamescope executable. The UGM PNG icon is
tracked at `system/icons/ubuntu-gaming-mode.png` and used by default.

```bash
GAMESCOPE_BIN=/path/to/gamescope \
bash packaging/build-deb.sh
```

The script rejects a Gamescope binary whose SHA256 is not authorized for the
package version. `UGM_ICON` can override the tracked icon when needed. Both
assets are included in the `.deb`; the Gamescope binary remains outside Git.
The build script compiles both NVIDIA and AMD VRS layers for 32/64-bit Vulkan
and generates manifests under `vrs-bundled`. A compiler and multilib support
are required; see each layer's README. Prototype build artifacts are ignored.

## Gamescope 3.16.30 GBM

- Upstream base: `ad2763da1c48860f649abfe842a087188dcb6e20`.
- Patches 0001–0008: upstream-derived GBM port.
- Patch 0009: UGM safety fallback.
- Final tree: `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.
- Binary SHA256:
  `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.

The tree and hash above apply **only to 2.5 (patches 0001–0009)**.
2.8 adds patch 0010 and pins Gamescope SHA256
`4251c390803678e1d3ce626ae4c36b51c00763338efa358ef1d2d5f5c8ef7c03`.
See the [2.8 build notes](docs/ugm-2.8/downsampling.md).

Historical 2.5 source preparation:

```bash
git clone https://github.com/ValveSoftware/gamescope.git
cd gamescope
git checkout ad2763da1c48860f649abfe842a087188dcb6e20
git submodule update --init --recursive
for patch in $(sed -n '1,9p' /path/to/gamescope/versions/3.16.30-gbm/patches/series); do git am "/path/to/gamescope/versions/3.16.30-gbm/patches/$patch"; done
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
