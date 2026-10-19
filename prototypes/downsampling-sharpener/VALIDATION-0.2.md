# Downsampling sharpening 0.2 — 13 October 2026 (historical)

**English** | [Español](VALIDACION-0.2.md) · [Current component](README_en.md)

## Actual state

Implemented, compiled and prepared for manual installation; **not installed
by the assistant** at that point. No Gamescope/game was running during the
work. A disappearance of stutter or restoration of roughly 25 FPS reported in
an earlier trial was **not verified**. VRR, synchronization, limiters, VRS and
user preferences were unchanged.

## Diagnosis

Version 0.1 sampled the game layer five times per pixel, including color
conversion, matrix, shaper and 3D LUT where active. It required full
composition on DRM/Wayland/OpenVR, preventing direct scanout even when scaling
might otherwise use it. The DRM path waits for composition (`vulkan_wait`).
In `steamcompmgr.cpp`, with tearing but without VRR, `WasCompositing()` changes
the flip from Async to Normal. A game's >120 FPS counter therefore proves
neither even presentation nor >120 distinct displayed frames.

Reducing shader cost targets a measured bottleneck; the compositing wait was
not removed, and tearing/VRR was not forced without evidence of user settings.
The base composition cost remains. VRR capability and a VRR request do not
prove it is active; inspect `GAMESCOPE_VRR_FEEDBACK`.

## Changes

- One pass: 8×8 block with 10×10 shared cache and cross halo, excluding
  corners: 96 samples for 64 pixels, versus 320. Color-transformed samples
  are reused, including in HDR. Shared memory is 1,600 bytes; no intermediate
  image or new CPU/GPU synchronization.
- Every thread, including border-fill threads, reaches the barrier; screen
  borders, rotations and overlays remain handled.
- `colorspace_passthru` uses direct access because tiling did not help the
  inexpensive RTX 2060 path.
- Contrast-adaptive strength attenuates near-flat regions and strong edges,
  retains a local RGB-range clamp and avoids filtering across the viewport.
  The 0–100 control and stored value are unchanged. No temporal history was
  added; motion stability needed visual gameplay testing.
- Decky shares a ten-second discovery cache between UI and synchronization,
  serializes operations with a lock and uses native `subprocess` credentials
  instead of thread-sensitive `preexec_fn`.

## Reproducible microbenchmark, not game FPS

RTX 2060; real Gamescope shader through Vulkan; synthetic 3840×2160 input to
2560×1440 output; strength 25; GPU timestamps; 160 warmed samples per variant
in interleaved rounds. Clocks were not locked and the desktop was active.
Identity LUTs and synthetic data do not reproduce a particular game's color
profile or workload. Presentation/KMS/VRR were not involved.

| Path | 0.1 median | 0.2 median | p95 0.1 → 0.2 |
| --- | ---: | ---: | ---: |
| SDR with color management/LUT | 6.665 ms | 2.448 ms | 7.744 → 3.481 ms |
| PQ input → SDR output, LUT | 6.667 ms | 2.466 ms | 7.736 → 3.497 ms |
| Unmanaged passthrough | 0.377 ms | 0.385 ms | 0.384 → 0.405 ms |
| Filter off, SDR composition | 1.378 ms | 1.379 ms | 1.962 → 1.851 ms |

The complete shader time fell **63% on the SDR/LUT path**. This is not 63%
more FPS or guaranteed savings on another GPU. Passthrough adaptation added
about 0.009 ms. The PQ case did not validate real HDR output. Full logs were
local under ignored `validation/results/final-*.log`. The loader printed
`Authorization required, but no authorization protocol specified` while
probing window platforms; the headless benchmark selected RTX 2060 and
completed Vulkan without error. Khronos Validation was not installed or run.

## Verification

- Gamescope compiled; `--version`, `git diff --check` and reverse patch check
  passed; three shaders passed `spirv-val --scalar-block-layout`.
- Tiled non-adaptive variant matched 0.1 byte-for-byte in tested paths,
  including 2560×1440 and 257×145, 90/180/270° rotations, overlay and shifted
  viewport. Off-filter final variant also matched 0.1 byte-for-byte.
- At strength 100, synthetic pattern stayed within the local range/gain bound
  (one-level quantization tolerance). This was not a visual motion assessment.
- Six controller tests, TypeScript and Decky build passed. Installer preflight,
  simulated hash-checked install, idempotence and rollback in a temporary tree
  passed without privilege.

## Historical installation and rollback

The 0.2 installer and payload paths in the original Spanish record referred to
local, ignored files and **are not distributed by this Git repository**. That
installer replaced six Gamescope/plugin files, verified source and prior
hashes, saved `/var/backups/ugm-sharpener-0.2.0-*` with `restaurar.py`, and
preserved preferences and attributes. It did not change VRS or restart a
service. A Gaming Mode logout/login was needed to load new Gamescope; an
already-running process kept its old binary. Its earlier 0.1 Decky repair
script was not for this update. This is history, not current install guidance.

## Pending smoothness test at the time

The planned controlled test used a fixed scene/camera/settings, recorded game,
render/output resolution, refresh, FPS cap and VRR, checked the running binary
hash, compared two 60-second off/on-25 frametime captures (including p95/p99)
in reversed order, separated composition cost from filter cost, and inspected
VRR feedback. It also compared strengths 0/25/50/100 on moving texture,
vegetation, game text, Steam/QAM, black bars and real HDR. Those steps were
not claimed as completed by this 0.2 record.
