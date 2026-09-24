# Ubuntu Gaming Mode source layout and reproducibility

**English** | [Español](SOURCE_es.md)

The files in this repository are organized from the validated Ubuntu Gaming Mode 1.0.0-3 GOLD package.

## Directories

- `src/`: UGM runtime scripts and session helpers.
- `system/`: systemd units, desktop/session entries and the Gamescope session configuration.
- `packaging/DEBIAN/`: Debian control and maintainer scripts.
- `packaging/build-deb.sh`: assembles a Debian package from the repository sources.
- `gamescope/`: information about the validated custom Gamescope runtime.
- `docs/`: installation, rollback, architecture and performance documentation.

## Binary inputs intentionally not stored in Git

The 1.0.0-3 package contains two binary assets that are not committed to the source history:

1. the custom Gamescope executable;
2. the UGM PNG application icon.

The GOLD `.deb` distributed through Releases contains both.

The build script therefore requires:

```bash
GAMESCOPE_BIN=/path/to/gamescope \
UGM_ICON=/path/to/ubuntu-gaming-mode.png \
bash packaging/build-deb.sh
```

For UGM 1.0.0-3, `build-deb.sh` refuses a Gamescope binary whose SHA256 differs from the validated one.

## Reproducibility note

The repository is now suitable as the source of future UGM package revisions, but **1.0.0-3 is not claimed to be byte-for-byte reproducible from Git alone**.

The Gamescope binary bundled with 1.0.0-3 was modified during UGM development for two project-specific requirements:

- correct 4K operation in the validated UGM environment;
- integration with the [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) Decky Loader plugin through a small connector used by the NIS workflow.

The modified Gamescope source tree and exact patch set were deleted after the validated binary had been produced. Because those sources no longer exist, this repository **does not claim to know the exact code changes** and will not attempt to recreate them from memory.

The current 1.0.0-3 binary and its SHA256 therefore remain the canonical reference. If a future UGM release requires a newer Gamescope version, the required 4K and Decky/NIS integration work will be recreated from the new upstream source and documented at that time.
