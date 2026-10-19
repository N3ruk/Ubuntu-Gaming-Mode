# Downsampling sharpening 0.3 — shader-only adjustment (historical)

**English** | [Español](VALIDACION-0.3.md) · [Current component](README_en.md)

## State

Installed on 13 October 2026 **without backups**; destination SHA256 checked
after atomic replacement. No Gamescope was active, so the new binary would
load at the next Gaming Mode login. Later HADES gameplay evaluation at
4K→1440p, HDR/VRR reported adequate sharpness and smoothness. Version 0.4's
later optimization is in [its record](VALIDATION-0.4.md).

Decky, its preferences and verified source/installed files were unchanged.
Relative to patch 0.2, only `src/shaders/cs_composite_blit.comp` changed:
no detection, composition, synchronization, VRR, scaler or VRS change.

## Patch 0010 adjustment

- Base gain changed from `0.8 * strength` to `2.4 * strength`.
- Maximum strong-edge attenuation changed from 50% to 35%; near-flat noise
  suppression remained.
- A strict clamp to the local min/max had cancelled sharpening at fine extrema.
  A bounded RGB overshoot is now allowed: 3.125% of local range at strength 25,
  up to 12.5% at 100. This bounds but does not guarantee invisible halos;
  moving-image review is required.
- The 96 samples per 64-pixel color-managed block, cache, barrier and single
  pass remain. Passthrough still uses direct samples. Steam/QAM is not filtered.
- Decky's saved value and control scale are unchanged. Start at 25 and compare
  off/25/50 in the same scene; strength 100 is not required for evaluation.

The canonical complete source is
[patch 0010](../../gamescope/versions/3.16.30-gbm/patches/0010-gamescope-downsample-sharpener.patch).
The current `series` appends it after the nine-patch 2.5 base. It is **not** an
incremental patch over 0.2; an older 0010 must be removed before applying it.

## Measured performance

RTX 2060 Vulkan microbenchmark, synthetic 3840×2160→2560×1440 at strength
25; 160 warmed samples per shader in interleaved rounds. Measures compute,
**not game FPS or presentation**.

| Path | 0.2 median | 0.3 median | Change | p95 0.2 → 0.3 |
| --- | ---: | ---: | ---: | ---: |
| SDR with LUT | 2.447488 ms | 2.449216 ms | +0.001728 ms | 3.121408 → 3.124832 ms |
| Passthrough | 0.389728 ms | 0.394592 ms | +0.004864 ms | 0.400224 → 0.403872 ms |

Cost was nearly equal in these tests, not guaranteed zero-overhead. The
passthrough shader rose about 1.25%. Reverse-order SDR repetition was
2.448288→2.450240 ms. An initial result concurrent with compilation and
unlocked clocks, 2.465888→2.906144 ms, was retained in local `sdr.log` but
not used as the stable result. Clocks and the user's session were untouched.
Other logs were local ignored artifacts. The loader emitted a window-platform
authorization warning, but headless RTX 2060 runs exited 0. No KMS/VRR or
physical HDR-output validation was performed.

## Image response and verification

In synthetic texture, mean difference versus off at strength 25 rose from
0.212 to 0.707 RGB 8-bit levels (~3.33×). This measures numerical effect,
**not 3.33× more quality or recovered detail**. Strengths 50 and 100 also
showed progressively stronger response.

- Shader passed `spirv-val --scalar-block-layout`; Gamescope compiled and
  `--version` worked. Off path matched 0.2 byte-for-byte; screen borders and
  alpha were intact.
- Overshoot stayed within defined bounds at 25/50/100 in linear light with
  quantization tolerance. Odd 257×145 dimensions, rotation, letterbox and
  overlay ran, but this was not a gameplay visual assessment.
- Patch applied/reversed on a clean base; `git diff --check` passed. Only the
  shader differed between 0.2 and 0.3 patches.
- Simulated install, hashes, idempotence and manual/failure rollback passed
  in a temporary tree without sudo. Decky hashes were unchanged.

Synthetic comparison image, metrics and bound-check scripts lived in ignored
local `validation/v0.3/`, not in Git.

## Historical local installation

The local no-backup installer lived in ignored `validation/v0.3/`. It replaced
only `/usr/lib/ubuntu-gaming-mode/gamescope`, checked hashes and retained owner,
permissions and attributes. It did not touch Decky or restart services.
Reinstalling the published UGM 2.5 package was the stated return route; the
older installer's rollback tests did not apply to this no-backup installer.
A Gaming Mode login loaded the binary. Gamescope 0.3 SHA256 was
`3260eaa4407623a02faed105dcfc2140be68ede64ac41e7bd979a41177cd1246`.
Controlled 0/25/50 moving-scene and frametime comparison remained pending;
the microbenchmark did not prove the gameplay result.
