# UGM 2.8.0-2 — release and feature overview

**English** | [Español](../es/ugm-2.8/README.md) · [Documentation index](../README.md)

The `2.8.0-2` package is published. This page describes what changed from
2.5.0-5; the component pages distinguish tests from hardware observations.

| Area | Change | Details |
| --- | --- | --- |
| Gamescope | Patch 0010: selective downsampling sharpening, filter 0.4; optional plugin control 0.2.0 | [Downsampling](downsampling.md) |
| Installer / Decky | Independent, default-off prompts for Decky Loader, Sharp Filter Selector and UGM Downsampling Sharpener; existing plugins are respected | [Decky](decky.md) |
| Bluetooth / USB wake | Default-deny USB wake on fresh installations that enable Bluetooth wake; individual controller authorization; 2.5 upgrade policy retained | [USB wake](usb-wake.md) |
| VRS | Shared 32/64-bit AMD/NVIDIA core, conservative compatibility policy, lower CPU overhead, GPL support and immutable staging | [VRS](vrs.md) |

Component-specific tests do **not** establish universal validation of the
complete package. Hardware behavior depends on the system and game.

## VRS source integration (17 October 2026)

Only VRS source files, scripts and tests were imported from the `UGM-vrs-audit`
worktree, including its uncommitted changes. Game captures, extracted shaders,
GPU profiling probes, rejected experiments and prebuilt libraries were not
imported. Packaging builds all four libraries from this repository. The exact
file list and hashes are in the [import manifest](../es/ugm-2.8/vrs-import-manifest.json).
Gamescope, patch 0010, Decky integration, wake policy and package metadata were
preserved during that import.

## Release validation and limits

- The first local package, 2.8.0-1, exposed a Decky permissions issue: UGM's
  `umask 077` left `PluginLoader` inaccessible to plugins.
- 2.8.0-2 runs the official Decky installer with `umask 022`, normalizes
  `PluginLoader` to `0755`, sets explicit plugin-payload permissions and
  reloads an active service. **82 automated tests** passed before publication.
- Not every hardware configuration, VRS path or USB wake path has been tested
  through a real suspend. Firmware and device behavior matter.
- The release `.deb` and `SHA256SUMS` are available on
  [GitHub](https://github.com/N3ruk/Ubuntu-Gaming-Mode/releases/tag/v2.8.0-2).
  `dist/` is a local build-output directory, not source tracked in Git.

[Changelog](../../CHANGELOG.md) · [Install](../installation.md) · [Source](../../SOURCE.md)
