# Gamescope 3.16.30 GBM scan-out port

**English** | [Español](README_es.md) | [Full technical audit (Spanish)](AUDIT.txt)

This directory preserves the reproducible Gamescope build used to restore stable NVIDIA 4K scan-out on the Ubuntu Gaming Mode reference system.

## Scope

- Upstream base: Gamescope 3.16.30, commit `ad2763da1c48860f649abfe842a087188dcb6e20`.
- Original GBM POC: ValveSoftware/gamescope commits `6d46d29..2bfc18c`, originally based on 3.16.25.
- Port result: `b211c9d3eab1310eda2e511c965da3cf5910290e`.
- Final Git tree: `b329fd0963e5ae797a14f4fe185f86c409937bc6`.
- Built version: `3.16.30-8-gb211c9d`.
- Validated binary SHA256: `96fbdd3f3c7e716e873cc66b79992f2b5e54d27c360ce4afb2c1bbb0628fffba`.

The feature is opt-in and UGM enables it with:

```bash
gamescope_drm_gbm_scanout=1
```

## What it changes

The clean path allocates output images through Vulkan and exports them as DMA-BUFs. This port lets the DRM backend allocate scan-out buffers through GBM and import them into Vulkan. The path was designed for drivers such as proprietary NVIDIA, where scan-out memory placement can differ from ordinary render allocations.

The existing Adaptive Sync/VRR implementation is not modified. The GBM path fixes the output-buffer allocation used by the existing 4K + VRR pipeline.

## What it does not contain

This source port does not contain the historical Sharp Filter Selector/Decky/NIS connector from UGM 1.0.0-3. The exact Gamescope 3.16.28 GOLD source remains unavailable and must not be conflated with this reproducible 3.16.30 port.

## Reproduction

See [UPSTREAM.md](UPSTREAM.md) for provenance, [BUILD.md](BUILD.md) for the exact build recipe and [VALIDATION.md](VALIDATION.md) for the acceptance scope.

Apply the patches in the order listed by `patches/series` to the exact upstream base. Applying all eight patches produces final tree `b329fd0963e5ae797a14f4fe185f86c409937bc6`.

The compiled binary is intentionally not committed to Git. It belongs in a versioned UGM release artifact and is identified by its SHA256.
