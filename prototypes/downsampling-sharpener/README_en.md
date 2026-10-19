# UGM Downsampling Sharpener — filter 0.4 / Decky 0.2

**English** | [Español](README.md) · [Prototype index](../README.md)

This component started as an experimental 2.8 prototype and is optionally
integrated in the published 2.8.0-2 package. Filter 0.4 preserves the image
of 0.3 while optimizing shader sample loads; it was locally installed and
SHA256-checked on 14 October 2026. Decky remains at 0.2. See the historical
validation records: [0.4](VALIDATION-0.4.md), [0.3](VALIDATION-0.3.md) and
[0.2](VALIDATION-0.2.md). For normal installation, use the
[package guide](../../docs/installation.md), not a local prototype installer.

## Behavior

- The Gamescope patch detects when the **composed focused-game surface** is
  reduced on both axes (`scale.x/y > 1.001`). It cannot detect scaling internal
  to a game or upscaler before the frame reaches Gamescope.
- A change to `cs_composite_blit.comp` applies bounded, contrast-adaptive
  unsharp-like sharpening **after downsampled game sampling and before Steam/
  QAM composition**. It does not change the scaler or FSR/NIS/SGSR sharpness.
- The independent Decky plugin offers an on/off switch (automatic activation
  only when downsampling is detected), strength 0–100 (default 25), and status.
  Without plugin activation, the filter stays off. Stored strength controls
  gain and overshoot bound; local contrast further modulates gain.
- Gamescope exposes `UGM_DOWNSAMPLE_SUPPORTED`, `UGM_DOWNSAMPLE_DETECTED` and
  `UGM_DOWNSAMPLE_ACTIVE` on each Xwayland root, accepting
  `UGM_DOWNSAMPLE_ENABLED` and `UGM_DOWNSAMPLE_STRENGTH` as 32-bit integers.
  Decky resends preferences when a session restarts.

## Reproducing a source build

Base: UGM Gamescope `3.16.30-gbm` at commit
`2dd5d3b0599eb2dda4dac5258aaec9374e1db16f`. Review before applying to
another revision. Patch 0010 is already present in the current package series;
**do not apply it twice**. In a clean, separate checkout of the nine-patch
base:

```bash
git apply --check /path/to/0010-gamescope-downsample-sharpener.patch
git apply /path/to/0010-gamescope-downsample-sharpener.patch
meson setup build-ugm-downsample --prefix=/usr/local --buildtype=debugoptimized
ninja -C build-ugm-downsample src/gamescope
```

The canonical patch is at
[`gamescope/versions/3.16.30-gbm/patches/0010-gamescope-downsample-sharpener.patch`](../../gamescope/versions/3.16.30-gbm/patches/0010-gamescope-downsample-sharpener.patch).
For the plugin, from `prototypes/downsampling-sharpener/decky/`:

```bash
pnpm install --frozen-lockfile
pnpm exec tsc --noEmit
pnpm build
```

`decky/` contains `main.py`, `plugin.json`, `package.json` and `dist/index.js`.
Keep `package.json` when installing the plugin: Decky needs its `type: module`
field to load the ES-module frontend. Building does not modify the installed
Gamescope.

## Historical hardware test plan and outcome

The original plan checked support/waiting state, detection at 3840×2160 →
2560×1440, off/on activation, equal-resolution deactivation, strengths
0/25/50/100, text and edge artifacts, Steam/QAM isolation, SDR/HDR/VRR and
aspect ratios. Active filtering requires composition and can lose direct
scanout; measure cost rather than assuming it is free.

Verified for 0.4: SHA256-checked local installation; SPIR-V validation and
identity with the measured shader; 27 byte-identical FP32 images versus 0.3;
lower shader GPU time on synthetic SDR/HDR tests; no material regression in
the off path. Decky was unchanged. A HADES gameplay observation reported
78–79% GPU load off and 81–82% on, with acceptable sharpness and smoothness.
It was **not** a controlled frametime benchmark and does not establish
universal game or hardware performance.
