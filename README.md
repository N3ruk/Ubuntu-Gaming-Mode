# Ubuntu Gaming Mode

**English** | [Español](README_es.md)

Ubuntu Gaming Mode (UGM) adds a dedicated Gamescope DRM/KMS + Steam Gamepad UI
session to Ubuntu Desktop. You can switch between the normal desktop and Gaming
Mode without replacing Ubuntu. The current stable package is **2.8.3-2** for
**Ubuntu 26.04 amd64**.

## Start here

1. Download the `.deb` and `SHA256SUMS` from the
   [latest release](https://github.com/N3ruk/Ubuntu-Gaming-Mode/releases/latest).
2. Follow the [installation and verification guide](docs/installation.md).
3. For operation, configuration and limitations, use the [documentation index](docs/README.md).

```bash
# From the directory containing both downloaded files:
sha256sum -c SHA256SUMS
sudo apt install ./ubuntu-gaming-mode_2.8.3-2_amd64.deb
sudo ubuntu-gaming-mode-doctor
```

Do not use a checksum file from another release. Installation/reconfiguration
offers optional features; Decky Loader, both Decky plugins, Bluetooth controller
wake and experimental VRS layers are not all enabled automatically.

## What 2.8 includes

- Dedicated Gamescope 3.16.30 GBM runtime with selective downsampling
  sharpening (patch 0010), optionally controlled by a Decky plugin.
- Optional Decky Loader, Sharp Filter Selector and UGM Downsampling Sharpener
  integration, with separate prompts and a permissions fix in 2.8.0-2.
- Bluetooth-controller wake option, including a default-deny USB wake policy
  **only for fresh 2.8 installations that enable it**. Upgrades from 2.5 retain
  their previous policy.
- AMD/NVIDIA 32/64-bit VRS layers with conservative eligibility and fallback;
  the native RADV APU route remains separate. VRS benefit and compatibility
  depend on the GPU, driver and game.
- Optional, default-off UGM-ICONS desktop theme, installed and activated for
  the configured user after an installer prompt.
- Desktop ↔ Gaming session switching, rollback, HDR/VRR preferences and Doctor.

Read the [2.8 feature overview](docs/ugm-2.8/README.md) for evidence and limits.
The [changelog](CHANGELOG.md) records previous versions.

## Project map

| Path | Purpose |
| --- | --- |
| [`docs/`](docs/README.md) | User guides, current features, architecture and historical records |
| [`src/`](src/) and [`system/`](system/) | Session/runtime tools and system integration |
| [`packaging/`](packaging/) | Debian metadata and build recipe |
| [`gamescope/`](gamescope/README.md) | Versioned Gamescope patch series and provenance |
| [`integrations/`](integrations/) | Optional Decky plugin and UGM-ICONS payloads |
| [`prototypes/`](prototypes/README.md) | Source, tests and historical research for experimental components |
| [`tests/`](tests/) | Automated project tests |

For contributors: [development guide](docs/development.md),
[source and reproducibility](SOURCE.md), and [third-party notices](THIRD_PARTY_NOTICES.md).
UGM's original code is [MIT licensed](LICENSE); bundled third-party components
retain their own licenses. UGM is independent of Valve, ChimeraOS, Bazzite,
NVIDIA and Canonical.
