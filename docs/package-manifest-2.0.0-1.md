# Ubuntu Gaming Mode 2.0.0-1 package manifest

## Components shipped in the package

- Validated Gamescope 3.16.30 GBM runtime, installed only at
  `/usr/lib/ubuntu-gaming-mode/gamescope`.
- `gamescope-session-plus` launchers and runtime.
- Desktop/Gaming selector, Gaming Mode return and autologin rearm tools.
- UGM Doctor and setup assistant.
- Managed Gaming session marker.
- Optional physical power-button bridge and `systemd --user` helper for
  Steam's native `steam://shortpowerpress` flow.
- Optional reversible helper to publish UGM's validated runtime as
  `/usr/local/bin/gamescope` without removing an existing Gamescope package.
- System and user systemd units.
- Wayland session definition, application launcher and icon.
- Steam Gaming Mode configuration for DRM, GBM scan-out, HDR, VRR, scaling
  and MangoApp.
- Debian scripts for installation, upgrade, removal, purge and restoration of
  the previous system state.

## Source material retained outside the binary DEB

The repository preserves the exact Gamescope base and reproducible series:

- upstream 3.16.30 base: `ad2763da1c48860f649abfe842a087188dcb6e20`;
- patches 0001–0008: upstream-derived GBM port;
- patch 0009: UGM safety fallback;
- build recipe, audit, validation record and checksums.

These source and documentation files are not installed on the target system;
they exist to reproduce and audit the bundled runtime.
