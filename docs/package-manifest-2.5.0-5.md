# UGM 2.5.0-5 package manifest (historical)

**English** | [Español](manifiesto-2.5.0-5.md) · [Documentation index](README.md)

This records the stable **2.5.0-5** package for Ubuntu 26.04 amd64, not the
current 2.8 payload or universal physical validation.

## Included in that package

- Gamescope 3.16.30 GBM, the same 2.0.0-1 executable, SHA256
  `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.
- `gamescope-session-plus` runtime, Steam DRM session, Desktop/Gaming Mode
  switchers, Doctor, setup, inventory and HDR/VRR preferences.
- NVIDIA and AMD Vulkan VRS layers for both 32- and 64-bit, with manifests,
  stored without global registration under
  `/usr/lib/ubuntu-gaming-mode/vrs-bundled/`. The installer records only the
  selected route; the session stages layers from the readable payload. The
  native RADV APU route uses no custom layer.
- Optional Bluetooth-controller wake app, icon, desktop entry, Polkit policy,
  reapply service and watcher for new controllers, all in the same `.deb` and
  hidden/disabled by default.
- Main UGM icon, systemd units, inventory autostart, Debian install/reconfigure/
  rollback scripts and user documentation.

## Source retained in Git

- `gamescope/versions/3.16.30-gbm/patches/`: patches 0001–0009 and `series`
  as the **historical 2.5 base**. The current series also appends patch 0010
  for 2.8; do not treat its full content as the 2.5 tree.
- `prototypes/{nvidia,amd}-vrs/`: `layer.c`, `spirv_policy.h`, `build.sh` and
  tests. Generated `.so`/JSON outputs and test binaries are not tracked.
- `system/icons/ubuntu-gaming-mode.png` and `system/icons/ugm-bt-wake.png`.
- `src/`, `system/`, `packaging/`, `tests/`, READMEs, source records, changelogs
  and `docs/`.

Gamescope executables and `.deb` files remain outside Git (`.build/` and
`dist/`). The current `packaging/build-deb.sh` targets 2.8 and checks its
Gamescope hash; it is **not** an unchanged recipe for a 2.5 package. VRS
compatibility depends on GPU, driver and game. See the
[2.5 release record](release-record-2.5.md).
