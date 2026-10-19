# UGM 2.8: selective downsampling sharpening (patch 0010)

**English** | [Español](../es/ugm-2.8/downsampling.md) · [2.8 overview](README.md)

**Status (14 October 2026): local validation completed.** Gamescope filter
0.4 and Decky plugin UGM Downsampling Sharpener 0.2 were tried in HADES. Image
sharpness and smoothness were judged satisfactory. At 3840×2160 input and
2560×1440 output with HDR, VRR and uncapped FPS, reported GPU-load readings
were **78–79% with the filter off and 81–82% with it on**. These are gameplay
observations, not instrumented measurements or proof of constant frametimes.
They cannot be converted into an exact percentage of total GPU work.

## Source layout

- [Patch 0010](../../gamescope/versions/3.16.30-gbm/patches/0010-gamescope-downsample-sharpener.patch)
  follows 0009 in the [series](../../gamescope/versions/3.16.30-gbm/patches/series).
  Apply it **once** to the nine-patch base, not on top of another 0010. It
  remains separate so the historical 2.5 binary identity stays unambiguous.
- [Implementation and plugin](../../prototypes/downsampling-sharpener/README_en.md);
  validation records for [0.4](../../prototypes/downsampling-sharpener/VALIDATION-0.4.md),
  [0.3](../../prototypes/downsampling-sharpener/VALIDATION-0.3.md) and
  [0.2](../../prototypes/downsampling-sharpener/VALIDATION-0.2.md). Heavy local
  test artifacts under `validation/` are not included in the `.deb`.
- The other optional plugin, **Sharp Filter Selector**, selects FSR/NIS/SGSR.
  It is not part of patch 0010; downsampling does not change its settings.

## Mechanism

1. Gamescope detects when the focused game's composed surface is downsampled
   on **both axes**. It cannot infer a game's internal resolution or upscaler.
   Support, detection and active-state properties are exposed on each Xwayland root.
2. Decky offers an on/off control and strength 0–100 (default 25). Without
   plugin activation, the filter stays off. The preference is forwarded when
   a new Gamescope session starts.
3. Adaptive sharpening runs on the game **after reduced sampling and before
   Steam/QAM composition**. Gain depends on local contrast and highlight
   bounds. FSR/NIS/SGSR sharpness, VRR, HDR and synchronization logic are not
   changed.
4. Version 0.4 cooperatively distributes the same 96 samples over each 8×8
   pixel block, removing redundant load branches without changing the samples
   or formula. The disabled-filter path is preserved.

## Evidence and limits

The 0.3→0.4 comparison produced **27 byte-identical FP32 images**, covering
synthetic SDR/HDR, strengths, rotations, edges, overlays and odd dimensions.
On an RTX 2060 Vulkan microbenchmark at 3840×2160→2560×1440 and strength 25,
median SDR→HDR shader time changed **2.456224 → 2.041728 ms (−16.88%)**;
incremental time against the disabled filter was about 1.077→0.663 ms. This
measures the shader, not a game's total GPU load. See the
[detailed results](../../prototypes/downsampling-sharpener/VALIDATION-0.4.md).

Version 0.4 was installed and checked by SHA256; no backups were created. The
HADES report indicated roughly 2–4 percentage points more GPU load. Other
games and hardware are not validated by this observation.

## 2.8 build

The package build requires an explicit version-specific Gamescope hash.
2.8.0-2 uses patches 0001–0010 and the 0.4 binary hash pinned in
`packaging/build-deb.sh`; it does not reuse the 2.5 binary. The measured
binary is recorded in [validation](../../prototypes/downsampling-sharpener/VALIDATION-0.4.md).
