# VRS layer included in UGM 2.8

**English** | [Español](../es/ugm-2.8/vrs.md) · [2.8 overview](README.md)

Integration date: 17 October 2026. The source is included in the published
2.8.0-2 package. This page preserves import provenance and the limits of its
validation.

## Provenance and scope

Source: `UGM-vrs-audit` worktree, HEAD
`9aa1262da897de1b478c38e276acf720d1fd32c1` **plus its uncommitted changes**.
The [SHA256 import manifest](../es/ugm-2.8/vrs-import-manifest.json) identifies
the exact files; the commit alone does not represent the imported state.

Included:

- `prototypes/vrs-common/`: core, SPIR-V policy, GPL support, diagnostics and tests.
- `prototypes/nvidia-vrs/` and `prototypes/amd-vrs/`: adapters, build scripts
  and original regression fixtures; libraries/manifests are built locally.
- `src/ugm-vrs-stage`, `src/ugm-vrs-select`, VRS-only changes in
  `src/ugm-capabilities` and the VRS section of `system/gamescope-session/steam`.
- Corresponding integration and capability tests.

The rest of the research worktree, `.build`, captures, timestamp/identity
probes, game shaders and rejected caches were not imported.
`src/ugm-vrs-activate` was already identical. Existing packaging invokes the
adapters; its choices, Gamescope, Decky, Bluetooth/USB wake and HDR/VRR policy
were not changed by this import.

## Improvements over this project's 2.5 layer

### 1. Shared core and lifecycle correctness

AMD and NVIDIA use the same implementation and policy, with adapters checking
vendor and driver. Per-instance/device dispatch, independent registries, full
handles on i386 and tracking of valid results in partially failing batches are
included. Allocation failures preserve the original path when instrumentation
cannot be done safely.

### 2. Lower layer CPU overhead

- Per-device hash tables replace linear shader/pipeline walks; less global
  contention and no unnecessary command-buffer tracking. Rate initialization
  still occurs at each bind.
- QAM reads use synchronous `statx` metadata; the file is reopened only when
  it changes. No polling or TTL is added. Invalid/special files or errors fall
  back to 1×1. The tested 32/64-bit runtime ABI remains compatible.
- The final GPL optimization removes **four temporary allocations per
  exclusively-GPL call/batch** and an empty lock. Batches are neither split nor
  reordered; allocator, arguments, `VkResult` and partial-success handles are
  preserved.

A historical simulated-driver microbenchmark measured approximately 47–48 ns
less for individual GPL cases on 64-bit, and 74–75 ns on 32-bit. Monolithic and
mixed controls had a small detection overhead (about 0.55–2.21% in that run).
**This does not measure driver compilation or prove less stutter, faster
startup or higher FPS.** Two experimental caches that regressed performance
were rejected and are not in this source.

### 3. More conservative shader policy

W=1, mixed paths and sensitive operations are protected, including FragCoord,
discard, explicit derivatives, depth, helper invocations, writes/atomics and
unknown semantics. Analysis bounds, blending/sample-shading exclusions and
protection of application-controlled VRS are retained. The layer neither
rewrites shaders nor maintains a game allowlist.

It also recognizes provable pure-helper forms, scalarized transforms, modern
declarative capabilities and storage roots with dynamic indices. Previously
observed Vulkan shader identifiers can recover analysis from a bounded
per-device table; unknown identifiers do not authorize coarse shading.

The position/W heuristic **is not an infallible interface detector**. More
protection can reduce coverage versus the old layer; benefit is not claimed
for all games.

### 4. Graphics Pipeline Library and flags2 compatibility

Conservative Graphics Pipeline Library (GPL) support resolves subsets, nested
links and local/linked state. Coherent private pre-rasterization/fragment
variants leave original libraries untouched. Missing information or failed
preparation preserves the original pipeline. Compatible flags2, including
descriptor-heap flags, are recognized only when the device actually supports
that route. Variants can increase compilation and memory cost.

### 5. Load and session integration

Builds publish libraries by rename; staging publishes complete, immutable
hash-addressed generations. Mapped libraries are not truncated and generations
used by live processes are not removed. `VK_LAYER_PATH`, `VK_ADD_LAYER_PATH`
and other explicit layers are respected. Several monitors on one verified GPU
no longer suppress VRS merely because display selection seems ambiguous;
HDR/VRR decisions are unchanged, and an ambiguous GPU remains ineligible.

### 6. Diagnostics and experiments, off by default

- `UGM_VRS_DEBUG=1`: decisions and exclusion reasons. Bind counts are **not**
  pixels, draws, GPU time or frame percentages.
- `UGM_VRS_AUDIT_ONLY=1`: retains the previous filter when comparing generic rules.
- `UGM_VRS_CAPTURE_DIR`: bounded local capture only when a directory is set;
  no capture is enabled or included in the imported project.
- `UGM_VRS_EXPERIMENTAL_RATE=1x2|2x1`: replaces only a QAM 2×2 request;
  QAM 1×1 still turns coarse shading off.
- `UGM_VRS_EXPERIMENTAL_MSAA=1`: opt-in static 2/4/8× MSAA tests, checking
  each rate/sample combination and keeping other protections; unsupported
  combinations fall back to 1×1.

**The public QAM remains 1×1/2×2, and extended MSAA is off by default.**
Prior research did not justify enabling it globally. No exemption was added
for the expensive Tomb Raider shader rejected on safety grounds.

## Validation and limits

Source research recorded a 32/64-bit CPU matrix and sanitizers, plus 72 GPU
cases for the last GPL change (48 ordinary and 24 with Khronos Validation
loaded) on NVIDIA RTX 2060. Those are historical results, not new tests of
2.8. AMD has builds and mocks but **no physical AMD GPU validation**.
Temporal quality and fixed-FPS power consumption remain unmeasured.

The final GPL optimization neither expands VRS coverage nor establishes a new
FPS gain. Copying source into the build project does not by itself prove a
game-performance gain.

### Import verification in this project

Executed on 17 October 2026:

- **66 source/script/test files** matched source SHA256 and permissions,
  including uncommitted changes. The audit source tree was retained.
- **77 project Python tests** passed.
- AMD/NVIDIA **32/64-bit CPU matrix**, optimized/unoptimized SPIR-V fixtures,
  AddressSanitizer and UndefinedBehaviorSanitizer passed.
- **60 NVIDIA RTX 2060 GPU cases** passed: 12 basic and 48 direct/nested GPL,
  audit and application-VRS cases on 32/64-bit. Khronos Validation was not
  rerun in this import; its earlier results are historical.
- The four VRS ELF libraries built with the same output options as
  `packaging/build-deb.sh` matched the audit worktree **byte for byte**;
  manifests were valid and both staging paths passed in an isolated `.build`
  directory.
- Script syntax and `git diff --check` passed. The 168 pre-existing files
  outside the import scope retained identical content and permissions.

[Results and hashes](../es/ugm-2.8/vrs-verification.json). Complete logs were
kept in the ignored local `.build/vrs-import-20261017/`. At import-verification
time, neither the complete `.deb` nor the new Gamescope had been built; both
were built later for the published 2.8.0-2 release.

### Reproduce from the project root

```sh
python3 -m unittest discover -s tests
bash prototypes/vrs-common/tests/run.sh
# Compatible NVIDIA GPU; prior fixtures needed:
bash prototypes/nvidia-vrs/tests/run-gpu.sh
bash prototypes/vrs-common/tests/run-gpl-gpu.sh
```

Requires 32/64-bit compilers/libc, Vulkan headers and loader,
`glslangValidator`, `spirv-val` and `spirv-as`. GPU tests are offscreen. Test
tools are not part of the `.deb` runtime payload; the package contains the
four libraries and their manifests, inert until the chosen session route
activates them. The [2.8 overview](README.md) distinguishes these component
tests from whole-package validation.
