# Gamescope bundled with Ubuntu Gaming Mode

**English** | [Español](README_es.md)

Ubuntu Gaming Mode 2.0.0-1 bundles a reproducible Gamescope 3.16.30 GBM build:

```text
version:  gamescope 3.16.30-8-gb211c9d
SHA256:   5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256
path:     /usr/lib/ubuntu-gaming-mode/gamescope
```

The compiled binary is distributed in the `.deb` and is not stored in Git.

## Reproducible source

[`versions/3.16.30-gbm/`](versions/3.16.30-gbm/README.md) contains:

- upstream base commit `ad2763da1c48860f649abfe842a087188dcb6e20`;
- eight upstream-derived GBM port patches;
- one separate UGM safety-fallback patch;
- exact application order;
- final source tree `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`;
- build recipe, toolchain, audit and validation records.

UGM enables the GBM path with `gamescope_drm_gbm_scanout=1`. It lets the DRM
backend allocate scan-out buffers through GBM and Vulkan import them, avoiding
the 4K corruption previously observed on the NVIDIA reference system. Patch
0009 prevents an upscale-semaphore import failure from bringing down the session
and falls back to normal composition.

## Scope

The series does not select scaling filters or change HDR/VRR behavior. Sharp
Filter Selector uses capabilities exposed by Gamescope/Steam rather than a
hidden connector in this patch series.

The historical Gamescope 3.16.28 used in UGM 1.0.0-3 cannot be reproduced
because its modified tree was lost. That limitation is confined to the old
release; the 3.16.30 runtime in 2.0.0-1 is fully documented.
