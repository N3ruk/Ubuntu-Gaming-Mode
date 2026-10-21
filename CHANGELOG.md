# Ubuntu Gaming Mode Changelog

**English** | [Español](CHANGELOG_es.md)

## 2.8.3-1 — 2026-10-21

- Clarify in the installer that the optional Sharp Filter Selector Decky plugin links QAM Sharp to SGSR when Gamescope supports it; Gamescope alone does not make QAM select SGSR. The plugin also selects NIS or forces FSR.
- Clarify the plugin description and bilingual documentation. No SGSR rendering logic changed.

## 2.8.2-1 — 2026-10-19

- Add a default-off installation prompt to install and activate UGM-ICONS for the UGM desktop user. Preserve an existing local theme and later user selections; restore the prior choice on opt-out/removal where appropriate. Icon files remain to protect user changes.
- Bundle the icon theme in the `.deb`; no additional download is required.

## 2.8.0-2

- Fix the Decky installer umask, `PluginLoader` and both plugin payload permissions; reload an active service.

## 2.8.0-1 — local package (unreleased)

### Preparation summary

- [Feature index and build prerequisites](docs/es/ugm-2.8/README.md) (Spanish).
- Previously prepared: Gamescope patch 0010 for selective downsampling sharpening
  (filter 0.4), plus optional Decky Loader and two-plugin integration with separate,
  default-off prompts. See [downsampling](docs/es/ugm-2.8/downsampling.md) and
  [Decky](docs/es/ugm-2.8/decky.md). This VRS import does not change those components.
- First local **2.8.0-1** `.deb` built on 2026-10-17; not published.
- Doctor matches Gamescope 0.4 identity, managed-state version and 2.8.0-1
  inventory cache; root is required to inspect private rollback state.
- 80 automated tests passed; package integrity and dependencies verified.
  A clean install exposed a Decky permissions issue, fixed in 2.8.0-2.

### Updated VRS layer — imported 2026-10-17

- Shared 32/64-bit AMD/NVIDIA core: per-instance/device dispatch, hash tables,
  partial-failure handling and conservative out-of-memory fallback.
- Lower CPU overhead for binds and synchronous QAM metadata checks; atomic library
  replacement and complete immutable staging generations.
- More protective SPIR-V policy with support for proven modern shader forms,
  previously observed shader identifiers and Graphics Pipeline Library (GPL)/flags2;
  original libraries remain intact, with coherent private variants for safe links.
- VRS selection independent of display ambiguity on a single verified GPU with
  multiple monitors; HDR/VRR behavior and other Vulkan loader layers are preserved.
- Final optimization removes four temporary allocations per exclusively GPL batch.
  This is a bounded CPU saving with a small non-GPL control overhead, not evidence
  of higher FPS or additional VRS coverage by itself.
- Public 1×1/2×2 QAM unchanged; 1×2/2×1 rates, extended MSAA, diagnostics and capture
  remain explicit opt-ins. No game probes or rejected experimental caches imported.
  AMD still lacks physical-GPU validation.
- Only VRS sources, integration and tests are imported. No local installation,
  Gamescope, Decky or Bluetooth changes and no GitHub publication.
- Verified in the destination project: 77 Python tests, 32/64-bit CPU matrix
  and sanitizers, 60 NVIDIA GPU cases and four libraries identical to the source.
- [Details, validation and limits](docs/es/ugm-2.8/vrs.md);
  [source manifest](docs/es/ugm-2.8/vrs-import-manifest.json).

### Bluetooth controller wake and USB policy

- On fresh installations that enable Bluetooth wake, `power/wakeup` is denied
  by default for physical USB devices that expose it, except the `btusb`
  adapter. Its shared hub/controller wake path remains enabled for Bluetooth.
- The policy covers USB devices already present and those connected later; it
  is reapplied at boot and when the Bluetooth adapter reconnects. USB mice,
  keyboards, receivers and controllers cannot be authorized in the app.
- On such fresh installations, paired Bluetooth controllers, including those
  paired before installing UGM, start with `WakeAllowed=no`. Controllers paired
  later also require individual authorization in the app. They must be trusted
  and expose `WakeAllowed`.
- Upgrading from 2.5 preserves the existing policy and permissions; USB
  default-deny is not enabled retroactively. When the option is disabled or
  UGM is removed, it attempts to restore the USB values it recorded, provided
  neither the device nor its state was changed elsewhere.
- Implementation and isolated tests are prepared. Building 2.8 and validating
  wake with a real suspend remain pending; final behavior also depends on
  firmware and hardware.

## 2.5.0-5 — 2026-10-12

- Retained the 2.0.0-1 Gamescope 3.16.30 GBM runtime and its nine-patch series;
  the Gamescope binary was not changed for 2.5.
- Added GPU/Vulkan inventory, HDR/VRR preferences and capability diagnostics.
- Added install-time VRS selection: a conservative native RADV APU route, or
  opt-in experimental AMD/NVIDIA layers. Both layers are built from C source
  for 32- and 64-bit Vulkan and bundled inertly.
- Added the optional Bluetooth controller-wake app, icon, Polkit policy and
  service in the same package; BlueZ is recommended and the feature defaults off.
- Fixed the VRS permission failure: the session stages the selected layer
  from readable `/usr/lib/ubuntu-gaming-mode/vrs-bundled` while keeping
  `/var/lib/ubuntu-gaming-mode` private. Steam receives the VRS signal only
  after the selected route has been staged successfully.
- Automated validation: 61 tests passed for the packaged release; two builds were byte-for-byte identical. Released as stable after maintainer validation.

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
