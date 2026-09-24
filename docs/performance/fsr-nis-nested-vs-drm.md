# FSR/NIS performance — nested Gamescope vs DRM Gaming Mode

Status: **documented from real project measurements**

Reference system:

- GPU: NVIDIA GeForce RTX 2060
- Desktop: Ubuntu / GNOME / Mutter / Wayland
- Gamescope: `3.16.28-3-g0d07f6e`
- Game: Dragon Ball Z: Kakarot
- Internal resolution: 1920×1080
- Output resolution: 2560×1440

## Goal

This document covers two project points:

1. Compare FSR/NIS performance between Gamescope nested inside the desktop and Gamescope running directly through DRM/KMS.
2. Explain why the performance cost of FSR/NIS is much higher inside GNOME/Mutter than in the dedicated Gaming Mode session.

## Measurements

### Gamescope nested — SDL backend

- Linear: ~96 FPS, approximately ±2 FPS
- FSR / NIS: ~88–91 FPS

Approximate filter penalty:

- 96 → 91 FPS: ~5.2%
- 96 → 88 FPS: ~8.3%
- Mean observed penalty: ~6.8%

### Gamescope nested — Wayland backend

- Linear: ~101–102 FPS
- FSR / NIS: ~88 FPS

Using ~101.5 FPS as the linear reference:

- Loss: ~13.5 FPS
- Approximate penalty: ~13.3%

### Native 2560×1440 without Gamescope

- ~70 FPS

This number is **not** used to calculate the cost of FSR/NIS because the game is rendering at a different internal resolution. It is only useful to show that rendering at 1080p and scaling to 1440p still provided a substantial performance advantage in this test.

### Gamescope DRM / Gaming Mode

- Linear: ~120 FPS
- FSR / NIS: ~114–117 FPS

Approximate filter penalty:

- 120 → 117 FPS: ~2.5%
- 120 → 114 FPS: ~5.0%
- Typical observed range: ~2.5–5%

## Nested → DRM performance gain

### Linear

- SDL nested: ~96 FPS
- Wayland nested: ~101–102 FPS
- DRM Gaming Mode: ~120 FPS

Approximate gain:

- DRM vs SDL: ~+25%
- DRM vs Wayland/Mutter: ~+18.2%

Observed range: **~18–25% higher performance**

### FSR / NIS

- SDL nested: ~88–91 FPS
- Wayland nested: ~88 FPS
- DRM Gaming Mode: ~114–117 FPS

Reasonable endpoint comparisons:

- 88 → 114 FPS: ~+29.5%
- 88 → 117 FPS: ~+33.0%
- 91 → 114 FPS: ~+25.3%
- 91 → 117 FPS: ~+28.6%

Observed range: **~25–33% higher performance**

These figures belong to this specific test and must not be treated as universal gains for every game, GPU or resolution.

## Why the difference exists

Gamescope has two fundamentally different operating scenarios.

### Dedicated DRM / Gaming Mode

Simplified path:

```text
Game
  -> Gamescope
  -> FSR/NIS when enabled
  -> DRM/KMS
  -> Display
```

In this mode Gamescope owns the DRM session and presents directly to the display path. It can avoid the desktop compositor in the final presentation chain and can make use of direct flips or hardware planes when available.

This is the scenario closest to SteamOS Gaming Mode.

### Nested inside GNOME/Mutter

Simplified path:

```text
Game
  -> Gamescope
  -> FSR/NIS
  -> Gamescope Wayland/SDL surface
  -> Mutter
  -> DRM/KMS
  -> Display
```

Here Gamescope does not directly control the final display path.

Two compositors are involved:

1. Gamescope composes and/or scales the game frame.
2. Mutter receives the resulting surface and integrates it into the desktop composition and presentation path.

That adds:

- another composition/presentation stage;
- synchronization between Gamescope and the host compositor;
- fewer opportunities for direct scanout/flip;
- extra work after the FSR/NIS processing has already completed.

FSR and NIS therefore still have their own processing cost, but in nested mode that processed frame also has to traverse the desktop compositor path.

## What the measurements show

The data is consistent with that model:

- Wayland linear: ~101–102 FPS
- Wayland FSR/NIS: ~88 FPS
- SDL linear: ~96 FPS
- SDL FSR/NIS: ~88–91 FPS
- DRM linear: ~120 FPS
- DRM FSR/NIS: ~114–117 FPS

FSR and NIS produced very similar performance in these tests. That suggests that a significant part of the observed penalty in this particular nested scenario comes from the presentation/composition path rather than only from the selected scaling algorithm.

This is an interpretation supported by the measurements; it does not prove that FSR and NIS have identical internal cost.

## Project conclusion

FSR and NIS are not considered broken in Ubuntu Gaming Mode.

The larger performance loss observed on the desktop belongs mainly to the nested path:

```text
Gamescope -> Mutter -> DRM
```

UGM was built specifically to provide:

```text
Gamescope -> DRM
```

The current implementation achieves that target and reproduces the fundamental architecture that makes Gamescope effective as the compositor of a dedicated gaming session rather than as a nested window inside a full desktop.

## Summary

| Scenario | Linear | FSR / NIS | Relative FSR/NIS penalty |
|---|---:|---:|---:|
| Nested SDL | ~96 FPS | ~88–91 FPS | ~5–8% |
| Nested Wayland/Mutter | ~101–102 FPS | ~88 FPS | ~13% |
| DRM / Gaming Mode | ~120 FPS | ~114–117 FPS | ~2.5–5% |

Nested → DRM observed gain:

- Linear: ~18–25%
- FSR/NIS: ~25–33%

## Technical references

- ValveSoftware/gamescope — README: https://github.com/ValveSoftware/gamescope
- ValveSoftware/gamescope — main/backends: https://github.com/ValveSoftware/gamescope/blob/master/src/main.cpp
- ValveSoftware/gamescope — Wiki: https://github.com/ValveSoftware/gamescope/wiki
