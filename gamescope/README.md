# Bundled Gamescope runtime

Ubuntu Gaming Mode 1.0.0-3 GOLD ships a validated custom Gamescope binary inside the release package.

Validated runtime:

```text
gamescope 3.16.28-3-g0d07f6e
```

SHA256:

```text
e43f0737287b2812d0c34a43c638131b3656058e1666d55228ad8acfe59d616c
```

Installed path:

```text
/usr/lib/ubuntu-gaming-mode/gamescope
```

The compiled binary is distributed in the `.deb` release rather than committed to Git history.

## Reproducibility status

The UGM 1.0.0-3 repository publishes the UGM source/configuration files extracted from the validated GOLD package.

A complete reproducible Gamescope build recipe (upstream commit, patches, build dependencies and exact build commands) is not yet recorded here. Until that is documented, the SHA256 above is the canonical integrity reference for the Gamescope binary bundled in 1.0.0-3.

Do not replace this binary in a 1.0.0-3 package without changing the package version and re-running the physical acceptance tests.
