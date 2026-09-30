# Install Ubuntu Gaming Mode 2.0.0-1 on Ubuntu 26.04

**English** | [Español](es/instalacion.md)

## Requirements

Ubuntu Gaming Mode 2.0.0-1 is packaged for **Ubuntu 26.04 amd64**.

The package includes the validated custom Gamescope runtime used by UGM and declares its runtime dependencies through APT.

## Verify the release

Download both files from the same GitHub Release:

- `ubuntu-gaming-mode_2.0.0-1_amd64.deb`
- `SHA256SUMS`

Verify the package before installation:

```bash
sha256sum -c SHA256SUMS
```

Expected result:

```text
ubuntu-gaming-mode_2.0.0-1_amd64.deb: OK
```

The release package SHA256 is:

```text
48779954b0bc4518d2fadeb29ba636e370c84a143317f19475b4cf3f5dea369f
```

## Install

```bash
sudo apt install ./ubuntu-gaming-mode_2.0.0-1_amd64.deb
```

During a first installation UGM:

1. detects the target desktop user and Ubuntu session;
2. creates an immutable snapshot of the pre-UGM state;
3. installs the dedicated Gamescope runtime and session integration;
4. configures GDM/AccountsService state required by UGM;
5. creates the managed-state for the installed version;
6. runs `ubuntu-gaming-mode-doctor` automatically.

The installer does **not** restart GDM and does **not** close the current desktop session.

## Console-style physical power button (optional)

Version 2.0.0-1 can optionally bridge a short physical power-button press to
Steam's native suspend flow. The option is **disabled by default** and captures
the button only while UGM's managed Steam Gaming Mode session is active. The
normal Ubuntu Desktop behavior remains unchanged.

The system daemon only captures and validates the button. Steam's fixed URI is
run by `ugm-steam-shortpowerpress.service` in the session's `systemd --user`,
so Steam's launcher does not inherit the system daemon sandbox.

The choice can be changed later and is fully reversible:

```bash
sudo dpkg-reconfigure ubuntu-gaming-mode
```

## UGM Gamescope as the system-wide command (optional)

The installer detects whether a `gamescope` command already exists and can
publish UGM's validated runtime through this link:

```text
/usr/local/bin/gamescope -> /usr/lib/ubuntu-gaming-mode/gamescope
```

The option is disabled by default. It does not uninstall or modify Ubuntu's
Gamescope package: `/usr/local/bin` normally takes precedence over `/usr/bin`.
UGM preserves the previous file or link state and restores it when the option
is disabled or the package is removed. The choice can also be changed with
`sudo dpkg-reconfigure ubuntu-gaming-mode`.

## Expected diagnostic result

A valid installation must finish with zero failures. The number of checks can
vary with hardware and selected options:

```text
Fallos:   0
```

The diagnostic can be run manually:

```bash
sudo ubuntu-gaming-mode-doctor
```

## Enter Gaming Mode

From Ubuntu Desktop, open:

**Volver a Gaming Mode**

UGM asks for confirmation before logging out of the desktop session.

The next GDM login enters the managed `steam-gaming-mode` session.

## Upgrade from 1.0.0-3

The package preserves UGM's original snapshot when upgrading from 1.0.0-3:

```bash
sudo apt install ./ubuntu-gaming-mode_2.0.0-1_amd64.deb
```

The upgrade preserves the immutable baseline and renews only the state managed
by the installed version. Review any installer warning and run Doctor when it
finishes.
