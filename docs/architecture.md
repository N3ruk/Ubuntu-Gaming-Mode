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

Validated version:

```text
gamescope 3.16.30-8-gb211c9d
```

Validated SHA256:

```text
5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256
```

This Gamescope binary is a project-specific, reproducible 3.16.30 build. Its
exact upstream base, nine-patch series, final tree, build recipe and validation
record are published under `gamescope/versions/3.16.30-gbm/`. Sharp Filter
Selector uses the Gamescope/Steam capabilities exposed by this runtime; the
patch series does not contain a hidden filter-selection connector.

The session wrapper/runtime is also installed under `/usr/lib/ubuntu-gaming-mode/`.

Validated runtime SHA256:

```text
3671cbc698b05e6ae3e68492848af3fec33aa0fe64c2f94a5e3e3395b498e636
```

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
