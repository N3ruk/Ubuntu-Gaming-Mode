# Downsampling sharpening 0.4 — lower cost, same image (historical)

**English** | [Español](VALIDACION-0.4.md) · [Current component](README_en.md)

## State and goal

Installed and SHA256-verified on 14 October 2026, **without backups**. Tests
ran on 13 October; 50 protected plugin files remained intact. The original
installation-result JSON was a local ignored artifact, not published in Git.

Version 0.3 gameplay evaluation had reported adequate sharpness and
smoothness. In HADES at 3840×2160 input, 2560×1440 output, HDR/VRR and
uncapped FPS, approximate GPU-load readings were 78% off and 88% on. These
were user-observed readings, not instrumented lab data. There was no capture
establishing the exact game colorspace, LUT, FPS or frametimes. The 0.4 goal
was to reduce GPU work without changing image or presentation; Decky,
detection and stored strength were left alone.

## Shader-only change to patch 0010

The **same 96 samples** are distributed cooperatively over each 64-pixel
block. One shared sampling loop replaces scattered edge and center loads.
Every used cache slot receives the same color-managed sample; four unused
corners are omitted.

- Same 8×8 blocks, `vec4[100]` cache, precision and barrier.
- Same curve, strength, gain, overshoot limits and letterbox protection.
- Same composition/overlay order; VRR, synchronization, cadence, scaling,
  HDR and color management are unchanged.
- Disabled-filter and passthrough paths do not use the new loop.
- No extension, subgroup dependency or NVIDIA-specific requirement is added.
- Active filtering still requires composition; its entire overhead cannot
  be promised away while keeping this filter.

A local 0.3→0.4 diff was kept under ignored `validation/v0.4/`. The complete
patch remains patch 0010 against base commit
`2dd5d3b0599eb2dda4dac5258aaec9374e1db16f`. Do not apply 0010 on top
of a different 0010 variant. The historical diff distinguished the shader
adjustment.

## Performance

RTX 2060; headless Vulkan microbenchmark; synthetic 3840×2160 input and
2560×1440 output; strength 25; five interleaved rounds (first discarded),
160 GPU timestamps per shader, clocks unlocked. This measures **shader
compute**, not FPS, total GPU load or game-presentation frametimes.

| Synthetic path | 0.3 median ms | 0.4 median ms | Reduction |
| --- | ---: | ---: | ---: |
| sdr | 2.431712 | 2.026784 | 16.65% |
| sdr-to-hdr | 2.456224 | 2.041728 | 16.88% |
| linear-to-hdr | 2.472672 | 2.126720 | 13.99% |
| scrgb-to-hdr | 2.380704 | 2.015456 | 15.34% |
| pq-to-hdr | 2.906112 | 2.129440 | 26.73% |
| passthrough | 0.393952 | 0.393504 | 0.11% |
| off-sdr | 1.378912 | 1.379040 | −0.01% |
| off-hdr | 1.379136 | 1.379072 | 0.00% |
| sdr-to-hdr-repeat | 2.457088 | 2.041600 | 16.91% |

For SDR→HDR, median changed **2.456224→2.041728 ms (−16.88%)**. Reverse-
order repetition was 2.457088→2.041600 ms (−16.91%); first-run p95 changed
2.458112→2.043424 ms. Relative to the off-filter shader in the same
microbenchmark, incremental cost fell about **1.077→0.663 ms (−38.5%)**.
This is not −38.5% of total HADES GPU load and cannot directly predict the
observed gameplay percentage.

The initial SDR round had a higher candidate p95 (2.515776 versus
2.433056 ms); it was retained. A repeat without concurrent compilation,
using SPIR-V extracted from the built binary, gave median
**2.438496→2.030592 ms** and p95 **2.447040→2.045728 ms**. No unverified
cause is assigned to the variation.

The manual-bilinear PQ scenario was another synthetic test, not a confirmed
HADES path. Test LUTs were not the monitor's. Performance used RGBA8 output;
separate RGBA32F comparisons checked differences hidden by quantization.
Neither physical 10-bit HDR output nor KMS/VRR was validated by this harness.
Performance JSON and logs were kept locally under ignored `validation/v0.4/`.
Window-platform probing produced the authorization warning; headless RTX
2060 tests exited 0.

## Equality and build checks

- **27 FP32 cases matched byte-for-byte, with no NaN/Inf**: colorspaces 0–4,
  SDR/PQ output, synthetic scRGB up to 1000 nits, variable alpha, strengths
  0/1/25/50/100, 90/180/270° rotation, letterbox, overlay, bilinear, inverse
  tone mapping and odd/smaller-than-block sizes down to 1×1.
- Nine performance cases and four exploratory/repeated comparisons also
  produced matching 8-bit images. Duplicate raw data were removed after
  recording equality and SHA256.
- `spirv-val --scalar-block-layout` passed. SPIR-V extracted from the actual
  compiled header was **identical to the measured shader**.
- Gamescope compiled and `--version` worked. In an isolated temporary
  repository the patch applied to the base, matched nine tree files and
  reversed exactly; `git diff --check` passed.
- 51 protected files were checked before installation: existing Gamescope
  and plugin source/installation. Decky was not written.
- Harness coverage added output EOTF, ITM, bilinear and FP32 in/out. No
  Khronos Vulkan Validation layer was installed; SPIR-V validation is **not**
  validation of the entire Vulkan API.

The original quality, performance and integration scripts/JSON were in local
ignored `validation/v0.4/`, not downloadable from this repository.

## Historical no-backup installation

The local payload was `.build/downsampling-sharpener/releases/0.4.0/0-gamescope`,
SHA256 `4251c390803678e1d3ce626ae4c36b51c00763338efa358ef1d2d5f5c8ef7c03`.
The ignored local installer atomically replaced only
`/usr/lib/ubuntu-gaming-mode/gamescope`, checking hashes and preserving owner,
permissions and attributes. It rejected an active Gamescope or changed target;
it made no backup and restarted no service. Reinstalling the published UGM 2.5
`.deb` was the stated rollback path. **These local prototype instructions are
not the current 2.8 installation procedure.**

## Later HADES observation

After 0.4 installation, gameplay evaluation still described acceptable
smoothness and sharpness. Under the same configuration, GPU-load readings were
**78–79% off and 81–82% on**. This was uncapped-FPS gameplay observation,
not instrumented frametimes or a direct conversion from shader benchmark to
total GPU load. No broader game/hardware conclusion follows from it.
