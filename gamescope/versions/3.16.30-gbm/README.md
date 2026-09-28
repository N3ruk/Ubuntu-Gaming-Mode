# Gamescope 3.16.30 GBM scan-out port

**English** | [Español](README_es.md) | [Full technical audit (Spanish)](AUDIT.txt)

This directory preserves the reproducible Gamescope build used to restore stable NVIDIA 4K scan-out on the Ubuntu Gaming Mode reference system.

## Scope

- Upstream base: Gamescope 3.16.30, commit `ad2763da1c48860f649abfe842a087188dcb6e20`.
- Original GBM POC: ValveSoftware/gamescope commits `6d46d29..2bfc18c`, originally based on 3.16.25.
- GBM port result (patches 0001-0008): `b211c9d3eab1310eda2e511c965da3cf5910290e`.
- Safety result (patch 0009): `190d2cf1edee4468c67ca30f2b55fd89289106fc`.
- Final Git tree: `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.
- Built version: `3.16.30-8-gb211c9d`.
- Validated binary SHA256: `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.

The feature is opt-in and UGM enables it with:

```bash
gamescope_drm_gbm_scanout=1
```

## What it changes

The clean path allocates output images through Vulkan and exports them as DMA-BUFs. This port lets the DRM backend allocate scan-out buffers through GBM and import them into Vulkan. The path was designed for drivers such as proprietary NVIDIA, where scan-out memory placement can differ from ordinary render allocations.

The existing Adaptive Sync/VRR implementation is not modified. The GBM path fixes the output-buffer allocation used by the existing 4K + VRR pipeline.

Patch 0009 is deliberately separate from the upstream GBM port. It handles a
real `vkImportSemaphoreFdKHR` failure in the pre-emptive upscale path: if an
acquire or release timeline semaphore cannot be imported, Gamescope disables
pre-emptive upscale for that session and continues through normal composition
instead of dereferencing a null semaphore. It does not change the selected
filter, VRR, HDR or the GBM scan-out path.

## What it does not contain

This source port does not contain the historical Sharp Filter Selector/Decky/NIS connector from UGM 1.0.0-3. The exact Gamescope 3.16.28 GOLD source remains unavailable and must not be conflated with this reproducible 3.16.30 port.

## Reproduction

See [UPSTREAM.md](UPSTREAM.md) for provenance, [BUILD.md](BUILD.md) for the exact build recipe and [VALIDATION.md](VALIDATION.md) for the acceptance scope.

Apply the patches in the order listed by `patches/series` to the exact upstream base. Applying all nine patches produces final tree `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.

The validated binary still reports `3.16.30-8-gb211c9d` because patch 0009
was compiled before its source diff was committed and exported as a numbered
patch. Its SHA256 above is the canonical identity of the installed nine-patch
runtime.

The compiled binary is intentionally not committed to Git. It belongs in a versioned UGM release artifact and is identified by its SHA256.
