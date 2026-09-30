# Ubuntu Gaming Mode Changelog

**English** | [Español](CHANGELOG_es.md)

## 2.0.0-1 — 2026-09-30

### Changed

- Prepared the package around the reproducible Gamescope 3.16.30 GBM runtime.
- Updated the package integrity checks and doctor for the validated runtime
  `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.
- Included the session-drain fix already present on the current main branch.
- Added an optional, reversible console-style physical power-button bridge.
  It is disabled by default, captures short presses only in the validated Steam
  Gaming Mode session and delegates suspension to Steam's native
  `steam://shortpowerpress` flow. Ubuntu Desktop behavior remains unchanged.
  The final dispatch runs through a `systemd --user` oneshot unit so Steam's
  32-bit launcher does not inherit the system daemon sandbox.
- Added an opt-in Debconf option to publish UGM's validated Gamescope runtime as
  the system-wide command through `/usr/local/bin/gamescope`. It does not remove
  the distribution Gamescope package, preserves the previous state and restores
  it when the option is disabled or UGM is removed.

### Validation status

- Clean installation, Desktop/Gaming transitions, physical power button, icon
  and Doctor were validated on the reference system.
- The global Gamescope option passed 8 deterministic creation, restoration and
  safe-rejection tests; two package builds were byte-for-byte identical.
- 11 physical-button tests and all 35 internal package hashes passed.

### Gamescope 3.16.30 source preservation

- Added patch 0009 to the reproducible GBM series. It falls back to normal
  composition when a pre-emptive-upscale timeline semaphore cannot be imported,
  rather than allowing a null dependency/signal to bring down the session.
- Preserved the safety change as commit `190d2cf`, with final source tree
  `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.
- Recorded the validated runtime SHA256
  `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.
- Documented the distinction between the eight upstream-derived GBM patches and
  the separate UGM safety patch, including the successful real failure fallback.

## 1.0.0-3 — GOLD

Production release promoted to GOLD after physical clean-install acceptance.

### Fixed

- Fixed `/etc/ubuntu-gaming-mode` potentially inheriting restrictive permissions during setup.
- The configuration directory is now explicitly created as `0755 root:root`.
- `session.conf` remains `0644 root:root`.
- Extended `ubuntu-gaming-mode-doctor` to verify that `session.conf` is readable by the configured UGM user.

### Validated

- Clean installation from a system with no previous UGM state and no Gamescope in PATH.
- Upgrade from 1.0.0-2 to 1.0.0-3.
- Automatic repair of the 1.0.0-2 permissions issue during upgrade.
- Immutable original snapshot preserved across upgrade.
- Purge restores the original baseline.
- Desktop → Gaming Mode → Desktop physical session cycle.
- 4K output.
- HDR.
- VRR / Adaptive Sync.
- Steam Overlay.
- Gamepad support.
- MangoApp / MangoHud.
- Gamescope and runtime SHA256 integrity.
- Final doctor result: **54 OK / 0 warnings / 0 failures**.

### GOLD artifact

```text
ubuntu-gaming-mode_1.0.0-3_amd64.deb
959afa06fd87eeeba8ffc1d4a3a6c965f9fa902d5678d43597b2ce7b20dca797
```

## 1.0.0-2

Pre-GOLD production candidate used for the first full physical acceptance pass.

The functional gaming stack passed 4K, HDR, VRR, gamepad and MangoHud testing, but physical Desktop → Gaming testing exposed a permissions bug in `/etc/ubuntu-gaming-mode`. That issue is fixed in 1.0.0-3.
