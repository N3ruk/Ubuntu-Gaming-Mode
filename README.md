# Ubuntu Gaming Mode

A SteamOS-style Gaming Mode session for **Ubuntu 26.04**, built around a dedicated **Gamescope DRM/KMS** session and Steam Gamepad UI.

Ubuntu Gaming Mode (UGM) switches between the normal Ubuntu desktop session and a console-style Steam session without replacing Ubuntu itself.

## Current release

**Ubuntu Gaming Mode 1.0.0-3 — GOLD**

Validated on Ubuntu 26.04 with an NVIDIA GeForce RTX 2060 using the bundled Gamescope runtime:

- Gamescope: `3.16.28-3-g0d07f6e`
- Dedicated DRM/KMS Gaming Mode session
- Desktop ↔ Gaming Mode switching
- 4K output
- HDR
- VRR / Adaptive Sync
- Steam Overlay
- Gamepad support
- MangoApp / MangoHud
- FSR scaling
- NIS scaling, unlocked through the [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) Decky Loader plugin

### Release checksum

```text
959afa06fd87eeeba8ffc1d4a3a6c965f9fa902d5678d43597b2ce7b20dca797  ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

## Installation

Download the `.deb` and `SHA256SUMS` from the GitHub Release, verify the checksum, then install:

```bash
sha256sum -c SHA256SUMS
sudo apt install ./ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

The package performs its own final diagnostic automatically during installation. You can run it again at any time with:

```bash
sudo ubuntu-gaming-mode-doctor
```

A healthy 1.0.0-3 installation reports:

```text
OK:       54
Warnings: 0
Fallos:   0
```

## Switching modes

From Ubuntu Desktop, launch **Volver a Gaming Mode**.

The launcher prepares the next GDM session, asks for confirmation, logs out of Ubuntu cleanly and enters the Steam Gaming Mode session.

Returning from Gaming Mode restores the configured Ubuntu desktop session.

## Removal

To remove UGM and restore the baseline captured before the first installation:

```bash
sudo apt purge ubuntu-gaming-mode
```

UGM keeps an immutable original-state snapshot so upgrades do not overwrite the rollback baseline.

See [docs/uninstall-and-rollback.md](docs/uninstall-and-rollback.md) for details.

## Performance

UGM exists specifically to avoid running Gamescope nested inside GNOME/Mutter.

Measured in Dragon Ball Z: Kakarot, 1920×1080 internal → 2560×1440 output:

| Scenario | Linear | FSR / NIS |
|---|---:|---:|
| Gamescope nested, SDL | ~96 FPS | ~88–91 FPS |
| Gamescope nested, Wayland | ~101–102 FPS | ~88 FPS |
| Gamescope DRM / Gaming Mode | ~120 FPS | ~114–117 FPS |

In this test, the dedicated DRM session delivered roughly **+18–25%** higher linear performance and **+25–33%** higher FSR/NIS performance than the tested nested paths.

Full methodology and interpretation: [docs/performance/fsr-nis-nested-vs-drm.md](docs/performance/fsr-nis-nested-vs-drm.md).

## Architecture

Simplified Gaming Mode path:

```text
Game
  -> Gamescope
  -> FSR/NIS when enabled
  -> DRM/KMS
  -> Display
```

This removes GNOME/Mutter from the final game presentation path.

See [docs/architecture.md](docs/architecture.md).

## Important behavior

UGM manages:

- the Steam Gaming Mode GDM/Wayland session;
- the selected AccountsService session;
- the autologin values required for Desktop ↔ Gaming switching;
- a minimal sudoers rule used only for the autologin rearm service;
- its own Gamescope runtime under `/usr/lib/ubuntu-gaming-mode/`.

Installation and upgrades do **not** restart GDM or close the current session.

## Source tree

The package sources are published in this repository:

- `src/` — UGM runtime scripts and session helpers
- `system/` — systemd units, desktop/session entries and Gamescope session configuration
- `packaging/DEBIAN/` — Debian control and maintainer scripts
- `packaging/build-deb.sh` — package assembly script
- `gamescope/` — validated custom Gamescope runtime information

See [SOURCE.md](SOURCE.md) for the source layout, binary inputs and reproducibility notes.

The compiled custom Gamescope binary and the application icon are intentionally not stored in Git history; they are inputs to the package build and are contained in the validated release artifact.

The Gamescope binary bundled in 1.0.0-3 is **not an unmodified upstream build**. During UGM development it was adapted to work correctly at 4K and to interoperate with the [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) Decky plugin through a small integration connector. The modified Gamescope source tree was later deleted, so the exact patch set can no longer be reconstructed reliably. That integration will be reimplemented and documented when UGM eventually needs to move to a newer Gamescope version.

## Documentation

- [Installation](docs/installation.md)
- [Uninstall and rollback](docs/uninstall-and-rollback.md)
- [Architecture](docs/architecture.md)
- [FSR/NIS: nested vs DRM performance](docs/performance/fsr-nis-nested-vs-drm.md)
- [Source layout](SOURCE.md)
- [Bundled Gamescope runtime](gamescope/README.md)
- [Changelog](CHANGELOG.md)

## Status

1.0.0-3 is the first release candidate promoted to **GOLD** after clean-install, upgrade, purge/rollback and physical Desktop ↔ Gaming acceptance testing.

## License

License information is not yet published in this repository. Third-party components retain their respective licenses.
