# Custom Gamescope runtime for Ubuntu Gaming Mode

**English** | [Español](README_es.md)

Ubuntu Gaming Mode 1.0.0-3 GOLD ships a validated **custom** Gamescope binary inside the release package.

This is not an unmodified upstream build. During UGM development it was modified to:

- work correctly with the project's validated 4K Gaming Mode configuration;
- interoperate with the [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) Decky Loader plugin through a small connector used by the NIS integration.

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

The modified Gamescope source tree used to produce the validated 1.0.0-3 binary was later deleted. As a result, the exact modifications, patch set and build recipe can no longer be recovered reliably and are intentionally **not reconstructed from memory** in this repository.

For 1.0.0-3, the validated binary and SHA256 above are therefore the canonical integrity reference.

There is no plan to reverse-engineer or approximate the lost patch set merely for documentation. When UGM eventually needs to update Gamescope to a newer version, the necessary 4K behavior and Sharp Filter Selector integration will be implemented again from the new source base and documented as part of that future release.

Do not replace this binary in a 1.0.0-3 package without changing the package version and re-running the physical acceptance tests.
