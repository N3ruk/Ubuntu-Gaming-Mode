# Install Ubuntu Gaming Mode

**English** | [Español](es/instalacion.md) · [Documentation index](README.md)

## Requirements

The published `2.8.3-1` package targets **Ubuntu 26.04 amd64**. Install it from
an interactive desktop terminal with APT, which resolves package dependencies.
Hardware-specific features (HDR, VRR, VRS and Bluetooth wake) depend on the
GPU, driver, monitor and firmware; see [known issues](known-issues.md).

## Download and verify

Download `ubuntu-gaming-mode_2.8.3-1_amd64.deb` and `SHA256SUMS` from the
[same GitHub release](https://github.com/N3ruk/Ubuntu-Gaming-Mode/releases/tag/v2.8.3-1).
In the download directory:

```bash
sha256sum -c SHA256SUMS
sudo apt install ./ubuntu-gaming-mode_2.8.3-1_amd64.deb
```

The checksum command should report `OK`. Keep the terminal open for Debconf
prompts. The installer records the original state for rollback, configures the
session and runs Doctor; it does not restart GDM or log out the current desktop.
An upgrade preserves the original baseline.

## Optional choices

A default-off prompt installs and selects UGM-ICONS for the configured
desktop user. The theme is bundled
and installed in `~/.local/share/icons/UGM-ICONS`. A pre-existing local folder
is preserved. On opt-out or package removal, UGM restores the previous icon
selection only if UGM-ICONS is still selected; files are retained to protect
user modifications. The choice can be revisited with
`sudo dpkg-reconfigure ubuntu-gaming-mode`.

Debconf offers separate, default-off choices for Decky Loader and its two
plugins; an existing Decky installation is respected. Installing Decky requires
network access, and failure of that optional step does not abort UGM. See
[Decky integration](ugm-2.8/decky.md).

Bluetooth controller wake and its app are optional. On **fresh 2.8 installs**
that enable wake, UGM applies the [USB wake policy](ugm-2.8/usb-wake.md).
Upgrades from 2.5 retain the earlier policy. A real suspend/resume test is
needed to establish support on your hardware.

The native RADV APU VRS route can be selected automatically on eligible hardware.
Custom AMD/NVIDIA VRS layers are experimental opt-ins. Neither a VRS toggle nor
a performance gain is guaranteed for every game. See [VRS](ugm-2.8/vrs.md).
The physical power-button bridge and global `gamescope` command are also
optional; the latter does not remove Ubuntu's Gamescope package.

To revisit install-time options:

```bash
sudo dpkg-reconfigure ubuntu-gaming-mode
```

## First use and diagnosis

Run `sudo ubuntu-gaming-mode-doctor` to inspect the installed runtime and
session integration. From Ubuntu Desktop, use **Volver a Gaming Mode** to prepare
the next GDM login; UGM asks before logging out. Leaving Gaming Mode returns to
the configured Ubuntu desktop session.

To remove UGM and restore the captured baseline:

```bash
sudo apt purge ubuntu-gaming-mode
```

See [uninstall and rollback](uninstall-and-rollback.md). For older versions,
consult the [historical records](README.md#previous-releases); do not use their
package filenames or hashes to install 2.8.
