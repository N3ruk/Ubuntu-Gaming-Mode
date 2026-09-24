# Ubuntu Gaming Mode Changelog

**English** | [Español](CHANGELOG_es.md)

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
