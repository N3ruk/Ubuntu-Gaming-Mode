# Validation record

## Reference system

- Ubuntu 26.04
- NVIDIA GeForce RTX 2060 6 GB
- Proprietary NVIDIA DRM/KMS path
- UGM canonical runtime: `/usr/lib/ubuntu-gaming-mode/gamescope`
- Feature flag: `gamescope_drm_gbm_scanout=1`

## Completed validation

- Source series applies cleanly to the exact 3.16.30 base.
- Patches 0001-0008 reproduce `b211c9d` exactly.
- The full nine-patch series reproduces `190d2cf` / tree `5fd5ea2` exactly.
- Release build completed successfully.
- Meson test suite: 68 passed, 0 failed.
- Installed UGM runtime hash matches the validated build.
- Physical 1440p output remains functional.
- Physical 3840x2160 output no longer exhibits the previously observed displaced-band scan-out corruption.
- 4K with the existing VRR/Adaptive Sync path was reported functional on the reference system.
- A real `vkImportSemaphoreFdKHR` failure was observed with Forgotten Anne. Patch 0009 emitted its fallback marker, disabled pre-emptive upscale for that session, kept Gamescope and the game running, and avoided the previous return to GDM.

## Interpretation

The patch does not add a new VRR implementation. It replaces the scan-out allocation route with backend-allocated GBM buffers imported into Vulkan. Existing 3.16.30 Adaptive Sync logic remains responsible for VRR.

Patch 0009 is a separate crash-safety change. It does not explain why GBM 4K +
VRR works; it explains why a failed timeline-semaphore import no longer brings
down the session while that working pipeline is in use.

## Acceptance matrix for a packaged release

Before promoting a new UGM package, record the result of each item:

| Mode | SDR | HDR | VRR | Steam UI | Result |
|---|---|---|---|---|---|
| 2560x1440 @ 60 Hz | pending package test | pending | pending | pending | pending |
| 2560x1440 @ 120 Hz | pending package test | pending | pending | pending | pending |
| 3840x2160 @ 60 Hz | observed functional | pending package test | observed functional | observed functional | partial |
| 3840x2160 @ 120 Hz, if display path permits | pending | pending | pending | pending | pending |

Also repeat clean installation, upgrade, package removal and rollback tests. This file deliberately distinguishes the successful live source/binary test from final package acceptance.
