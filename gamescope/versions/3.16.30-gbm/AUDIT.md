# Technical audit: Gamescope 3.16.30 + NVIDIA GBM port

**English** | [Español — original detailed audit](AUDIT.txt) · [Gamescope source index](README.md)

**Historical record, 28 September 2026.** This English audit consolidates the
findings of the original Spanish research log. Its original publication
checklist describes work planned *at the time*; the 2.0, 2.5 and 2.8 releases
have since been published. The 2.8 package adds patch 0010, which is outside
the nine-patch scope audited here.

## Purpose and evidence boundary

The audit compared clean Gamescope 3.16.30 at upstream commit
`ad2763da1c48860f649abfe842a087188dcb6e20`, a local 3.16.30 tree with
the GBM port, and the stored patch series. It checked provenance, exact
reconstruction, build/test outcome, installed binary identity and which change
plausibly explains stable 4K scanout with VRR on the NVIDIA reference system.
The comparison did not modify the original trees or binaries; a temporary
worktree used for reconstruction was removed after checking.

## Result and identities

| Item | Identity |
| --- | --- |
| Clean 3.16.30 base | `ad2763da1c48860f649abfe842a087188dcb6e20` |
| Original Valve GBM POC base (3.16.25) | `17baf4abd1ab3353fb705e4d0d023f84e870f7e8` |
| Original Valve GBM POC result | `2bfc18c736520b7d4f9756977213ea439daa1c63` |
| Ported 3.16.30 GBM result, patches 0001–0008 | `b211c9d3eab1310eda2e511c965da3cf5910290e` |
| UGM safety result, patch 0009 | `190d2cf1edee4468c67ca30f2b55fd89289106fc` |
| Final nine-patch tree | `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1` |
| Nine-patch Gamescope binary SHA256 | `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256` |

The tracked diff from base to result touched **10 files**, with **652
insertions** and **62 deletions**. SHA256 of the complete binary diff was
`09169ae06f222ff5b382cf2e00d2e63d76b7e6bbc78ae7aa5e9e21545ff53773`.
Generated Meson files (`subprojects/.wraplock` and, on the clean tree,
`subprojects/glm/`) were untracked and excluded. The patched source HEAD used
a local branch; the original patch archive was not itself a Git repository
at audit time.

The GBM series is hosted in ValveSoftware's official Gamescope repository but
was an **experimental POC**, not a feature of the standard 3.16.30 tag. The
first eight patches were ported from a 3.16.25 base to 3.16.30; patch 0009
is UGM's independent safety fallback. The exact commit-by-commit provenance
is in [UPSTREAM.md](UPSTREAM.md). No Decky Loader, NIS, FSR, Sharp Filter
Selector, VRR or session-configuration changes were found in the patched
Gamescope tree.

## Reconstruction and build

Applying patches **0001–0008** to the exact upstream base reproduces the GBM
port; applying **0009** produces tree
`5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`. The source version after
0009 is `3.16.30-9-g190d2cf`; the validated executable still reports
`gamescope 3.16.30-8-gb211c9d` because patch 0009's diff was committed after
that binary was built. Use SHA256, not the embedded version string alone, for
binary identity. Checksums of the individual stored patches are in
[SHA256SUMS](SHA256SUMS); the canonical order is in [patches/series](patches/series).

The historical toolchain included Meson 1.10.1, Ninja 1.13.2, Ubuntu GCC/G++
15.2.0-16ubuntu1, GBM 26.0.8-1ubuntu0.3, `-O3`, no LTO and no stripping.
`HAVE_GBM=1` was set. **68/68 Meson tests** passed on the eight-patch GBM
baseline. Patch 0009 was subsequently compiled as a Gamescope target, but no
second full Meson suite run was recorded after that one-file change. See the
[build recipe](BUILD.md) and [validation record](VALIDATION.md); do not infer
unrecorded test results.

## Why the GBM port mattered for 4K + VRR

The default output-image path allocates via Vulkan and exports DMA-BUFs. On the
proprietary NVIDIA reference stack, scanout-capable placement can differ from
ordinary render allocation. The GBM port lets the DRM backend allocate the
scanout buffer and import it into Vulkan, addressing the observed displaced
4K band. Existing Gamescope Adaptive Sync logic still controls VRR; the port
does **not** implement a new VRR algorithm. This is a coherent explanation
for the observed 4K/VRR behavior on the reference system, not a proof for all
drivers or displays.

## Patch 0009: separate failure containment

A real `vkImportSemaphoreFdKHR` failure (`VkResult: -3`) occurred in the
pre-emptive upscale path during Forgotten Anne on 28 September 2026. Before
0009, a missing imported timeline semaphore could be used as a null
acquire/release dependency and crash Gamescope. Patch 0009 imports both
semaphores before constructing the command buffer; if either is missing, it
logs the failure, disables pre-emptive upscale for the rest of that session
and continues normal composition. Gamescope and the game remained active;
no segmentation fault, core dump or return to GDM followed. 4K, VRR and
scaling continued in that observed session. The patch does **not** change
GBM, VRR, HDR or filter selection, and it did not create the 4K+VRR path.

## Limits and relation to later releases

This audit identifies a reproducible 3.16.30 GBM/safety series. It does
**not** reconstruct the lost modified Gamescope 3.16.28 tree used in UGM
1.0.0-3 or its former Decky/NIS connector. The nine-patch binary hash above
belongs to 2.0/2.5. The [2.8 downsampling patch](../../../docs/ugm-2.8/downsampling.md)
adds 0010 and has its own hash; do not apply the old hash or patch count to
2.8. Package acceptance across all resolutions, refresh rates, HDR and VRR
combinations was not established by this source audit. Refer to
[VALIDATION.md](VALIDATION.md) for the actual scope rather than treating
pending checklist entries in the original log as passed.
