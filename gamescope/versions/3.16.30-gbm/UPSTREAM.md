# Upstream provenance and commit map

## Repositories and bases

- Repository: `https://github.com/ValveSoftware/gamescope.git`
- Clean UGM port base: `ad2763da1c48860f649abfe842a087188dcb6e20` (Gamescope 3.16.30).
- Original GBM POC base: `17baf4abd1ab3353fb705e4d0d023f84e870f7e8` (Gamescope 3.16.25).
- Original GBM POC result: `2bfc18c736520b7d4f9756977213ea439daa1c63`.
- Local 3.16.30 port result: `b211c9d3eab1310eda2e511c965da3cf5910290e`.
- Local safety result: `190d2cf1edee4468c67ca30f2b55fd89289106fc`.

## Commit map

| Original POC | 3.16.30 port | Subject |
|---|---|---|
| `6d46d29` | `6359a7f` | build: add optional gbm dependency |
| `a86849b` | `c52eda7` | backend: add IBackendScanoutBuffer and scanout allocation hooks |
| `ebbaf8b` | `96d612f` | drm: implement GBM-allocated scanout buffers (opt-in) |
| `b384ac4` | `1d55bd5` | rendervulkan: import backend-allocated (GBM) scanout buffers |
| `977e4f5` | `7f93559` | gitignore: ignore meson-extracted glm subproject directory |
| `6605942` | `b5996b6` | drm, rendervulkan: address GBM scanout review feedback |
| `092a2e7` | `a924b3c` | rendervulkan, steamcompmgr: address second review round |
| `2bfc18c` | `b211c9d` | drm, rendervulkan: apply final review findings |

The table above is the complete upstream GBM port. Patch 0009 has no upstream
POC counterpart: it is a UGM safety fix added after a real timeline-semaphore
import failure caused the pre-emptive upscale path to crash.

## 3.16.30 integration

`git range-diff` reports commits 1, 2, 5 and 8 as patch-equivalent. Commits 3, 4, 6 and 7 retain overlapping work already present in 3.16.30, including output rotation, HDR-capability rebuilds and `GetQueryPoolResults`, while integrating the GBM series.

No Decky, NIS, FSR or VRR feature patch is present in the tracked diff. The only
post-port UGM change is patch 0009, which provides fail-safe fallback to normal
composition and does not add or select a scaling filter.

## Reproducibility proof

The nine stored patches were applied with `git am` to a temporary worktree at the exact 3.16.30 base. The resulting tree was:

```text
5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1
```

This matches the tree of local result commit `190d2cf` exactly. Patches 0001-0008 alone still reproduce `b211c9d` / tree `b329fd0`; patch 0009 is separately identifiable. The temporary worktree was removed after verification.
