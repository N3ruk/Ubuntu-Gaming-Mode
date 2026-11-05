# UGM Add Non-Steam Content

[Español](README_ES.md)

**Included as the Non-Steam panel in UGM Options, UGM 2.8.5.**
The standalone source is retained as an assembly input, not a second installed entry.
Based on [Add Non-Steam Games](https://github.com/danielcamilo1/decky-add-non-steam-games).
See [NOTICE.md](NOTICE.md), [LICENSE](LICENSE), and [ORIGIN.json](ORIGIN.json).

## Included

- Installed Linux applications from XDG desktop entries, including Flatpak/Snap
  exports. Honors precedence, localized names, Hidden/NoDisplay, TryExec,
  desktop visibility, Path, argument boundaries and supported Exec field codes.
- File browser: user places, mounted-media folders, absolute paths, hidden-file
  toggle, search and pagination. ELF binaries, AppImages, shebang scripts and
  Windows EXEs are inspected without executing them.
- Reviewed name, working directory, launch arguments and optional Proton tool.
  Windows EXEs require an explicitly selected available compatibility tool.
- Steam's live shortcut API; duplicate destination/arguments checks and readback
  of name, executable, working directory, arguments and selected tool.
- Open the library entry; explicitly confirmed undo of this operation's entry
  only, provided its identity still matches. No file/prefix/save deletion.
- Read-only on-disk check for the active SteamID provided by the frontend. Never
  guesses another account or writes shortcuts.vdf. Disk status does not certify
  Proton settings, game launch or survival of a future client restart.
- Draft/operation state survives QAM remounts in memory, and clears when the
  Steam account changes. No private paths saved in localStorage or diagnostics.

## Boundaries

No Steam restart, session-wrapper changes, installers, launcher discovery,
artwork API keys, networking in the Python backend, mounting, sudo or root.
The only backend write is the explicit **Allow execution** action, which adds
owner-execute to a regular file owned by the current user, without following a
symlink. `plugin.json` has no `root` flag; the backend refuses UID 0.

Unsupported desktop entries (terminal-only, DBus-only or unknown Exec syntax)
are excluded instead of being guessed. MSI/BAT/CMD, shortcuts and data/archive
files are not supported manual targets. Adding a game is not installing it;
launcher games may require their original runner, environment and prefix.
The interface currently uses Spanish and Decky's standard gamepad components.

Internal Steam APIs are version-sensitive: unavailable APIs, incomplete library
reads and unknown client responses produce an error, not a silent VDF fallback.
A failed/incomplete creation is shown separately from a verified result. When
Steam does not expose a selected Proton tool by name, verification stays incomplete.
On-disk saving may remain pending until Steam exits normally; this plugin does
not force a restart to try to flush the library.

## Development and independent artifact

```sh
npm ci --ignore-scripts
npm run typecheck
npm test
python3 -m unittest discover -s tests -p 'test_*.py'
npm run build
python3 build.py
```

`build.py` creates only `release/UGM-Non-Steam-1.0.0-dev.zip`, with a fixed file
allowlist and SHA256 manifest. It does not touch UGM's DEB recipe, root dist,
Decky installation, Steam library or services. Tests use fixtures/mocks, including
React interactions with simulated Decky controls; they do not prove actual
Decky styling, controller focus, Steam compatibility, launches or persistence.

## UGM Options integration

The master retains this engine and panel, running its worker as the Decky user,
not root. Real read-only RPCs have been checked; launches, controller navigation
and saving across Steam sessions still require live validation.
[Current UGM guide](../../../docs/ugm-2.8.5/README.md).
