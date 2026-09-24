# Ubuntu Gaming Mode for Ubuntu 26.04

**English** | [Español](README_es.md)

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

## NVIDIA support: Gamescope DRM 4K, HDR and VRR validated on RTX 2060

UGM 1.0.0-3 was developed and physically accepted on an **NVIDIA GeForce RTX 2060 6 GB** using the proprietary **NVIDIA 595.91.07** driver on Ubuntu 26.04.

The validated dedicated Gamescope DRM/KMS session includes:

- 4K output;
- HDR;
- VRR / Adaptive Sync;
- Steam Overlay;
- gamepad support;
- MangoApp / MangoHud;
- FSR and NIS scaling;
- Desktop ↔ Gaming Mode switching.

This matters because NVIDIA remains a more demanding path in the SteamOS/Gamescope ecosystem than the typical AMD configuration:

- upstream Gamescope supports the proprietary NVIDIA driver, but explicitly requires a sufficiently recent driver and DRM KMS/modesetting support ([Gamescope README](https://github.com/ValveSoftware/gamescope));
- Bazzite currently classifies NVIDIA Steam Gaming Mode as supported with **major caveats** compared with AMD. Its own Gaming Mode documentation says that enabling Steam's **GPU accelerated rendering in web views** improves NVIDIA UI performance, while warning that the same option will **most likely cause game-breaking graphical artifacts** ([hardware compatibility](https://docs.bazzite.gg/Gaming/Hardware_compatibility_for_gaming/), [Gaming Mode quirks](https://docs.bazzite.gg/Handheld_and_HTPC_edition/quirks/));
- Bazzite issue reports also show concrete NVIDIA regressions seen in the field: Gaming Mode failing to stay running on an RTX 4070 while desktop Steam still works, severe lower-screen flickering after an NVIDIA driver/library mismatch on a GTX 1660 SUPER, and a hybrid RTX 4050 system where an external DisplayPort monitor works in Desktop Mode but receives no signal in Gaming Mode ([RTX 4070 report](https://github.com/ublue-os/bazzite/issues/5032), [GTX 1660 SUPER report](https://github.com/ublue-os/bazzite/issues/3092), [RTX 4050 external-display report](https://github.com/ublue-os/bazzite/issues/2611));
- ChimeraOS removed NVIDIA support for roughly 1.5 years because of the state of NVIDIA drivers, Wayland and Gamescope, reintroduced the drivers while still warning of **significant performance and stability issues**, and later officially supported GTX 16-series-and-newer hardware while still stating that **Steam UI performance is poor**. Its troubleshooting page specifically recommends enabling Steam's GPU-accelerated web rendering to reduce NVIDIA UI lag ([release notes](https://github.com/ChimeraOS/chimeraos/wiki/Release-Notes), [troubleshooting](https://github.com/ChimeraOS/chimeraos/wiki/Troubleshooting));
- a 2026 upstream Gamescope report documents persistent **4K DRM scan-out corruption** on NVIDIA: displaced vertical framebuffer bands/columns appear on screen at 3840×2160 @ 30/60/120, while 1080p/1440p remain clean. The report was reproduced on RTX 4080 SUPER and RTX 4070 systems and also remained visible with static content and force-composition enabled ([Gamescope issue #2309](https://github.com/ValveSoftware/gamescope/issues/2309)).

By contrast, the UGM RTX 2060 reference system has physically validated a **smooth Steam Gamepad UI at both 60 FPS and 120 FPS** in Gaming Mode, in addition to the 4K/HDR/VRR tests above. No UI-lag problem or game-breaking graphical artifacts were observed during that acceptance testing.

UGM does **not** claim universal NVIDIA compatibility or that these upstream/distribution-specific problems are fixed for every GPU. The narrower, tested claim is that the RTX 2060 reference system successfully runs a standalone Gamescope DRM session at 4K with HDR, VRR, a smooth Steam UI at 60/120 FPS and the Steam Gaming Mode features listed above.

For NVIDIA users who already run Ubuntu, this is also a practical alternative to replacing the operating system with a dedicated gaming distribution: UGM adds a separate console-style Gamescope session while preserving the existing Ubuntu desktop and driver stack.

The bundled Gamescope build was specifically adapted during development for the validated 4K path and the Sharp Filter Selector integration. Its modified source tree was later deleted, so the exact Gamescope-side changes are not reconstructed or claimed here.

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

- [Installation](docs/installation.md) · [Español](docs/es/instalacion.md)
- [Uninstall and rollback](docs/uninstall-and-rollback.md) · [Español](docs/es/desinstalacion-y-rollback.md)
- [Architecture](docs/architecture.md) · [Español](docs/es/arquitectura.md)
- [FSR/NIS: nested vs DRM performance](docs/performance/fsr-nis-nested-vs-drm.md) · [Español](docs/es/rendimiento-fsr-nis-gamescope-nested-vs-drm.md)
- [Source layout](SOURCE.md) · [Español](SOURCE_es.md)
- [Bundled Gamescope runtime](gamescope/README.md) · [Español](gamescope/README_es.md)
- [Changelog](CHANGELOG.md) · [Español](CHANGELOG_es.md)
- [v1.0.0-3 GOLD release notes](docs/releases/v1.0.0-3.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md) · [Español](THIRD_PARTY_NOTICES_es.md)

## Status

1.0.0-3 is the first release candidate promoted to **GOLD** after clean-install, upgrade, purge/rollback and physical Desktop ↔ Gaming acceptance testing.

## License

UGM's original project code is released under the [MIT License](LICENSE).

Third-party components keep their own licenses and copyright notices. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), including the ChimeraOS gamescope-session MIT notice and the Gamescope BSD 2-Clause license.
