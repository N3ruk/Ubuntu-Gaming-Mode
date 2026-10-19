# Ubuntu Gaming Mode architecture: Gamescope DRM/KMS on Ubuntu 26.04

**English** | [Español](es/arquitectura.md)

Ubuntu Gaming Mode provides a dedicated Steam/Gamescope session alongside the normal Ubuntu desktop.

## Desktop path

A normal Ubuntu desktop gaming path is conceptually:

```text
Game
  -> Gamescope (when nested)
  -> Wayland/SDL surface
  -> GNOME Mutter
  -> DRM/KMS
  -> Display
```

When Gamescope is nested, Mutter remains in the final presentation path.

## UGM Gaming Mode path

UGM instead starts Gamescope as the compositor for the gaming session:

```text
Game
  -> Gamescope
  -> FSR/NIS when enabled
  -> DRM/KMS
  -> Display
```

This removes GNOME/Mutter from the final game presentation path.

For NIS, the UGM workflow uses the [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) Decky Loader plugin to unlock/select the filter in Gaming Mode.

## Canonical runtime

UGM uses its own validated Gamescope binary:

```text
/usr/lib/ubuntu-gaming-mode/gamescope
```

The published **2.8.0-2** package uses the 3.16.30 GBM base plus
**patch 0010** for selective downsampling sharpening. The expected Gamescope
binary SHA256 pinned in the package build is:

```text
4251c390803678e1d3ce626ae4c36b51c00763338efa358ef1d2d5f5c8ef7c03
```

The historical **2.5** runtime used patches 0001–0009 and SHA256
`5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.
Its embedded version string was `gamescope 3.16.30-8-gb211c9d`.
That hash is **not** the 2.8 binary identity. See the
[Gamescope source record](../gamescope/README.md) and
[2.8 downsampling details](ugm-2.8/downsampling.md). Sharp Filter Selector
uses Gamescope/Steam capabilities exposed by the runtime; the GBM patch
series does not contain a hidden filter-selection connector.

The session wrapper/runtime is also installed under `/usr/lib/ubuntu-gaming-mode/`.
Its content changed across releases; compare hashes with the matching
published package, not a prior version's manifest.

## DRM scanout

The validated Steam session preserves:

```bash
export gamescope_drm_gbm_scanout=1
```

The DRM connector is detected automatically rather than hard-coded to a particular connector such as `HDMI-A-1`.

## Steam session

UGM launches Steam in a console-oriented configuration using the Gamepad UI and SteamOS-oriented flags.

The package provides:

```text
/usr/share/wayland-sessions/steam-gaming-mode.desktop
```

## Desktop ↔ Gaming switching

UGM uses AccountsService to select the session for the next GDM login.

The Desktop → Gaming launcher:

1. verifies it is running from the configured desktop session;
2. asks the user for confirmation;
3. selects `steam-gaming-mode`;
4. rearms the required autologin state through a tightly scoped system service;
5. logs out of GNOME cleanly.

The Gaming → Desktop path selects the configured Ubuntu desktop session before returning.

## Privilege boundary

The normal session-switching flow runs as the desktop user.

Only the specific system operation needed to rearm autologin is exposed through a minimal sudoers rule.

## Diagnostics

`ubuntu-gaming-mode-doctor` validates the package runtime, Gamescope hashes, session configuration, systemd units, sudoers, GDM, AccountsService, rollback state, DRM, GPU and Steam availability.

It also verifies that `session.conf` is readable by the configured UGM user,
the packaged runtime hashes, optional physical-button integration and the
optional system-wide Gamescope link.
