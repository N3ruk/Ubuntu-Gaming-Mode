# Uninstall and rollback

UGM records the state that existed before the first installation and uses it as the rollback baseline.

## Purge

For a complete removal:

```bash
sudo apt purge ubuntu-gaming-mode
```

A validated purge of 1.0.0-3 restores the original baseline and removes the UGM state directory.

On the clean reference system this restored:

- AccountsService session to `ubuntu`;
- the original GDM state;
- removal of `/etc/ubuntu-gaming-mode` because it did not exist before UGM;
- removal of `/var/lib/ubuntu-gaming-mode`;
- removal of package-managed UGM files.

## Original snapshot

The first installation creates an immutable original-state snapshot under:

```text
/var/lib/ubuntu-gaming-mode/original-state
```

Upgrades preserve this baseline instead of replacing it with the state of the previous UGM version.

The snapshot includes an integrity manifest and completion marker.

## Managed state

The currently installed UGM version is tracked separately under:

```text
/var/lib/ubuntu-gaming-mode/managed-state
```

This allows the package to distinguish:

- the original system state;
- the state managed by the currently installed UGM version.

## Important

Do not manually modify the GDM autologin/session values managed by UGM unless you intend to repair them afterwards. Reinstalling or upgrading UGM may overwrite managed values to restore a valid configuration.

For diagnostics before removal:

```bash
sudo ubuntu-gaming-mode-doctor
```
