# Upstream provenance and commit map

## Repositories and bases

- Repository: `https://github.com/ValveSoftware/gamescope.git`
- Clean UGM port base: `ad2763da1c48860f649abfe842a087188dcb6e20` (Gamescope 3.16.30).
- Original GBM POC base: `17baf4abd1ab3353fb705e4d0d023f84e870f7e8` (Gamescope 3.16.25).
- Original GBM POC result: `2bfc18c736520b7d4f9756977213ea439daa1c63`.
- Local 3.16.30 port result: `b211c9d3eab1310eda2e511c965da3cf5910290e`.

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

## 3.16.30 integration

`git range-diff` reports commits 1, 2, 5 and 8 as patch-equivalent. Commits 3, 4, 6 and 7 retain overlapping work already present in 3.16.30, including output rotation, HDR-capability rebuilds and `GetQueryPoolResults`, while integrating the GBM series.

No additional UGM, Decky, NIS, FSR or VRR feature patch is present in the tracked diff.

## Reproducibility proof

The eight stored patches were applied with `git am` to a temporary worktree at the exact 3.16.30 base. The resulting tree was:

```text
b329fd0963e5ae797a14f4fe185f86c409937bc6
```

This matches the tree of local result commit `b211c9d` exactly. The temporary worktree was removed after verification.
