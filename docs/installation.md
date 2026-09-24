# Installation

## Requirements

Ubuntu Gaming Mode 1.0.0-3 is packaged for **Ubuntu 26.04 amd64**.

The package includes the validated custom Gamescope runtime used by UGM and declares its runtime dependencies through APT.

## Verify the release

Download both files from the same GitHub Release:

- `ubuntu-gaming-mode_1.0.0-3_amd64.deb`
- `SHA256SUMS`

Verify the package before installation:

```bash
sha256sum -c SHA256SUMS
```

Expected result:

```text
ubuntu-gaming-mode_1.0.0-3_amd64.deb: OK
```

The GOLD package SHA256 is:

```text
959afa06fd87eeeba8ffc1d4a3a6c965f9fa902d5678d43597b2ce7b20dca797
```

## Install

```bash
sudo apt install ./ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

During a first installation UGM:

1. detects the target desktop user and Ubuntu session;
2. creates an immutable snapshot of the pre-UGM state;
3. installs the dedicated Gamescope runtime and session integration;
4. configures GDM/AccountsService state required by UGM;
5. creates the managed-state for the installed version;
6. runs `ubuntu-gaming-mode-doctor` automatically.

The installer does **not** restart GDM and does **not** close the current desktop session.

## Expected diagnostic result

For the validated 1.0.0-3 configuration:

```text
OK:       54
Warnings: 0
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

## Upgrade from 1.0.0-2

1.0.0-3 was explicitly tested as an in-place upgrade from 1.0.0-2:

```bash
sudo apt install ./ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

The upgrade preserves the immutable original snapshot and updates only the managed state.

It also fixes the 1.0.0-2 permissions issue where `/etc/ubuntu-gaming-mode` could be created as `0700 root:root`. In 1.0.0-3 it is explicitly created as `0755 root:root`, while `session.conf` remains `0644 root:root`.
