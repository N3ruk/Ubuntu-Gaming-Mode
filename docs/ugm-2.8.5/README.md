# Ubuntu Gaming Mode 2.8.5

**English** | [Español](../es/ugm-2.8.5/README.md) · [Documentation](../README.md)

Package **2.8.5-1**, Ubuntu **26.04 amd64**. This page describes changes since
2.8.3-2; existing Gamescope, VRS, Bluetooth and icon-theme features are documented
in the [2.8 overview](../ugm-2.8/README.md), not new features of this release.

## Steam fixes

- **YouTube with a controller:** pause/resume is synchronized with player readiness,
  retaining playback position instead of recreating the player.
- **Steam client updates:** the update button queries the official manifest for
  the selected branch and requests only a client update through Steam's native
  updater. It does not request an OS/BIOS update, change branches or reboot Ubuntu.
  A reported client version `0` is accepted only when three pinned client file
  hashes identify the known build, not as proof that an update is available.

Both hooks are independent of Decky and of each other. They require **Node.js
>=22** from APT and support Steam build **1792618285**, with additional factory/
file-hash and API guards. Other builds are rejected until reviewed; installing
UGM does not imply compatibility with every future Steam update.

The package starts the user services with the managed UGM Steam session and
stops them on exit. Before starting Steam it prepares the native installation's
`.cef-enable-remote-debugging` marker for the existing CDP interface on
`127.0.0.1:8080`. It removes only an unchanged marker created by UGM. Pre-existing
or modified markers remain; removing a marker does not close an already-running
Steam debugging interface. No Steam files are patched, and Steam is not restarted
by installation. An already-running client may need a normal exit/restart to
activate debugging. Flatpak Steam is not covered by this integration.

Exact legacy checkout-generated hook units are migrated conservatively. Custom
units, masks and drop-ins are preserved and reported as conflicts, not overwritten.

## UGM Options

One optional **UGM Options 1.0.0** entry contains four separate panels:

| Panel | Purpose |
| --- | --- |
| Sharp Filter | Restores NIS/FSR choices and enables QAM sharpening with SGSR on supported Gamescope. |
| Downsampling | Uses UGM's existing Gamescope patch for adjustable sharpening when internal resolution exceeds monitor resolution. |
| Video Output | Lists detected screens and selects the Gaming Mode video output; no audio, resolution or refresh-rate controls. |
| Non-Steam | Adds installed Linux applications or manually selected executables to Steam, with reviewed launch settings and optional Proton for Windows EXEs. |

Sharp Filter and Downsampling retain their existing engines and controls. Grouping
these panels does not add new rendering algorithms. Video Output retains guarded
EDID/connector checks; actual GPU/dock support needs hardware testing. Non-Steam
checks duplicates and reads back its operation; adding a shortcut does not install
a game or establish Proton compatibility. Its disk check reads only the active
account's shortcuts and does not write VDF files or force Steam to restart.

The master uses separate workers: only Video Output retains root. Sharp,
Downsampling and Non-Steam run as the Decky user before their code is imported.
Settings remain under the original module settings directories.

## Installation and upgrade

Follow the [installation guide](../installation.md). Decky Loader and UGM Options
have separate optional prompts, off by default on fresh installs. Each module's
purpose is explained in English and Spanish. An earlier module selection can
preselect the master only if its new prompt has not already been answered.

Accepting UGM Options stages the verified master outside Decky's discovery tree,
stops Decky briefly and retires detected copies of the four standalone plugins,
including renamed folders identified by plugin identity. Their code is retained
under `~/homebrew/ugm-options-migration/`, outside `plugins/`. Settings and unrelated
plugins are preserved. A failed replacement/restart restores the previous folders;
a service previously stopped is not started. Declining does not remove legacy
plugins. Unknown identities, linked folders and modified masters are preserved
with a warning instead of being overwritten. An unrelated `output-manager` is
not removed and prevents a second video writer.

UGM package removal does not uninstall Decky or delete user plugin settings.
[Decky source/build](../../integrations/decky/ugm-options/README.md).

## Diagnosis and validation limits

`sudo ubuntu-gaming-mode-doctor` checks the packaged payload against dpkg, Node.js,
UGM unit linkage, recent hook status while UGM is active, legacy unit conflicts,
UGM Options and duplicate plugin entries. It is read-only: it does not connect to
CDP, query updates, load plugins, switch screens or start/stop services. Stopped
hooks outside UGM and an absent optional plugin are valid states.

Automated verification covers hook contracts, session lifecycle and marker cleanup,
plugin isolation/navigation, transactional migration, package staging and integrity.
The local master has been registered in real Steam/Decky and all four modules
answered read-only RPCs. Earlier human testing confirmed the YouTube fix and Steam
update checks. These are not equivalent to a full installed-DEB session test.

Still pending: real controller navigation of the master, physical display switching,
Non-Steam launch/Proton/persistence across Steam sessions, and a complete update
with a genuinely newer Steam client. No FPS or universal hardware claims are made.
[Release verification](VERIFICATION.md).
