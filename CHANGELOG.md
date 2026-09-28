# Ubuntu Gaming Mode Changelog

**English** | [Español](CHANGELOG_es.md)

## Unreleased

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
