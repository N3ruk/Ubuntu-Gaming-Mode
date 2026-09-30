# Ubuntu Gaming Mode for Ubuntu 26.04

**English** | [Español](README_es.md)

**Ubuntu Gaming Mode (UGM)** adds a console-style Gamescope DRM/KMS and Steam
Gamepad UI session to Ubuntu Desktop. It switches between the normal desktop
and Gaming Mode without replacing Ubuntu with another distribution.

## Current release

**Ubuntu Gaming Mode 2.0.0-1**

- Ubuntu 26.04 amd64.
- Gamescope `3.16.30-8-gb211c9d` with a reproducible GBM port.
- 4K output, HDR and VRR/Adaptive Sync validated on an NVIDIA RTX 2060.
- Steam Overlay, gamepad and MangoApp/MangoHud.
- FSR, NIS and SGSR scaling, with optional Quick Access Menu control through
  [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector).
- Desktop ↔ Gaming Mode switching through GDM and AccountsService.
- Optional console-style physical power-button suspension.
- Optional reversible publication of UGM's Gamescope as the global command.
- Original-state snapshot, rollback and integrated Doctor.

The `.deb` and its `SHA256SUMS` are published together in the GitHub release.

## What's new in 2.0.0-1

### Reproducible Gamescope 3.16.30

UGM ships a dedicated runtime at:

```text
/usr/lib/ubuntu-gaming-mode/gamescope
```

Its source is reconstructed from upstream commit
`ad2763da1c48860f649abfe842a087188dcb6e20` and the versioned nine-patch series
in [`gamescope/versions/3.16.30-gbm/`](gamescope/versions/3.16.30-gbm/README.md).
The patches produce final tree
`5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.

The first eight patches port the GBM scan-out-capable buffer path. The ninth
adds a safe fallback when pre-emptive-upscale semaphore import fails. On the
NVIDIA reference system this preserves a stable 4K session with HDR and VRR.

### Sharp Filter Selector: SGSR, FSR and NIS from QAM

UGM 2.0.0-1's Gamescope 3.16.30 exposes the modern scaling properties used by
[Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector), an
optional Decky Loader plugin that adds explicit **Use FSR** and **Use NIS**
controls to the Quick Access Menu while preserving Gamescope's native Sharp
path. Download the current packaged plugin from its
[releases page](https://github.com/N3ruk/Sharp-Filter-Selector/releases/latest).

Together they provide:

- native **SGSR** for SDR application input when both overrides are disabled;
- Gamescope's native **FSR fallback** when the application supplies HDR input;
- explicit FSR or NIS selection and a shared 0–5 sharpness control;
- correct availability based on the active application's HDR feedback, not
  merely on whether the display output is in HDR mode; and
- synchronization across Gamescope's Xwayland roots without overriding the
  scaler selected by Steam/QAM (`Auto`, `Fit`, `Integer` or `Stretch`).

The connection is deliberately narrow: Steam/QAM continues to own
`GAMESCOPE_NEW_SCALING_SCALER`; the plugin reads the active Gamescope/Xwayland
session and writes only the filter/sharpness properties such as
`GAMESCOPE_NEW_SCALING_FILTER`, `GAMESCOPE_SHARP_FILTER` and
`GAMESCOPE_FSR_SHARPNESS`. Gamescope then performs the actual scaling. The
plugin does not patch, replace or install Gamescope, and UGM does not bundle or
install Decky Loader or the plugin.

### Console-style physical power button

The installer can optionally enable a short physical power-button press that
hands off to Steam's native `steam://shortpowerpress` flow. It is **disabled by
default**, only captures the button in the managed Gaming Mode session and
leaves normal desktop behavior unchanged.

### System-wide Gamescope command

Another opt-in setting can create:

```text
/usr/local/bin/gamescope -> /usr/lib/ubuntu-gaming-mode/gamescope
```

It does not uninstall Ubuntu's Gamescope package. UGM preserves the previous
state and restores it when the option is disabled or the package is purged.

Both choices can be changed later:

```bash
sudo dpkg-reconfigure ubuntu-gaming-mode
```

## Installation

Download the `.deb` and `SHA256SUMS` from the release, then run:

```bash
sha256sum -c SHA256SUMS
sudo apt install ./ubuntu-gaming-mode_2.0.0-1_amd64.deb
```

The installer creates the original snapshot, configures the session and runs
Doctor. It does not restart GDM or close the current session.

Manual diagnostics:

```bash
sudo ubuntu-gaming-mode-doctor
```

See the [installation guide](docs/installation.md).

## Switching modes

From Ubuntu Desktop, launch **Volver a Gaming Mode**. UGM prepares the next GDM
session, asks for confirmation and logs out cleanly. Leaving Gaming Mode restores
the configured Ubuntu desktop session.

## Removal and rollback

```bash
sudo apt purge ubuntu-gaming-mode
```

UGM restores the baseline captured before the first installation. See
[uninstall and rollback](docs/uninstall-and-rollback.md).

## Validated platform

Physical acceptance was primarily performed on Ubuntu 26.04, an NVIDIA RTX
2060 6 GB and proprietary NVIDIA driver 595.91.07. UGM does not claim universal
GPU compatibility; NVIDIA remains a more demanding Gamescope path than the
typical AMD configuration.

## Performance

UGM avoids running Gamescope nested under GNOME/Mutter. In the measured Dragon
Ball Z: Kakarot 1080p internal → 1440p test, the dedicated DRM session delivered
roughly 18–25% higher linear performance and 25–33% higher FSR/NIS performance
than the tested nested paths. See the
[full methodology](docs/performance/fsr-nis-nested-vs-drm.md).

## Known Steam issue

Some Steam Big Picture notifications show a black rectangle on the tested
NVIDIA stack. It also reproduces on the desktop without Gamescope, and the
surface reaches the compositor already opaque, so UGM currently has no safe
fix. See [known issues](docs/known-issues.md).

## Source and reproducibility

- `src/`: UGM runtime and helpers.
- `system/`: systemd units, session files and Gamescope configuration.
- `packaging/`: Debian metadata and package build.
- `gamescope/versions/3.16.30-gbm/`: patches, provenance, build and validation.
- `tests/`: deterministic tests for optional integrations.

The Gamescope binary and application icon are not stored in Git; they are
included in the versioned `.deb` and verified by SHA256. See [SOURCE.md](SOURCE.md).

## Acknowledgements

UGM builds on Valve/SteamOS/Gamescope and ChimeraOS' `gamescope-session-plus`
session infrastructure. It also acknowledges the documentation and work of
Bazzite and the wider Linux gaming community. UGM is independent and is not
affiliated with or endorsed by Valve, ChimeraOS, Bazzite, NVIDIA or Canonical.

## License

UGM's original code uses the [MIT License](LICENSE). Third-party components
retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
