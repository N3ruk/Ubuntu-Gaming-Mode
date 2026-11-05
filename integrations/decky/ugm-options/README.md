# UGM Options

**1.0.0, included in UGM 2.8.5**. One Decky entry, four unchanged plugin panels:
Sharp Filter, Downsampling, Video Output and Non-Steam. [Español](README_ES.md).

## Scope

A navigational container, not a new settings engine. Each panel keeps its own
controls, validations, background tasks and preferences. No Apply All, shared
profiles, new scaling options or Steam restart. The home screen does not mount
any module panel. Back returns to the home screen; returning from a Non-Steam
subscreen first uses that module's existing navigation.

The original frontend bundles and Python engines are copied byte-for-byte under
modules/. During assembly, the master frontend redirects only Decky's plugin
name/callable transport to the master broker. No global Decky API patch, eval,
iframe, duplicate React or dependence on separately installed plugins.
manifest.json pins every reused file. The build rejects unrecognized bridges.

## Architecture / permissions

Decky starts the master broker with root (needed by the existing Video Output
engine). The broker creates one persistent subprocess per module with a private
stdio channel and an explicit RPC allowlist in modules.json. Only Video Output
retains root. Sharp, Downsampling and Non-Steam start with the Decky user's actual
UID/GID/groups **before importing their code**. Non-Steam still rejects root.
Failures are isolated; timed-out writes are never retried automatically.

Requires /usr/bin/python3 and the same runtime requirements as the original
plugins. No new service, root command endpoint, exposed TCP port or sudo shell.
Individual module unload hooks run when Decky unloads the master, not when moving
between panels. Opening/backing out of a panel does not reset its engine.

## Settings and migration

The broker reuses settings/<original-plugin-slug>/ without conversion or resets.
The UGM installer offers Decky Loader and this master, not four separate plugins.
On accepting UGM Options, ugm-decky-integration validates and stages the complete
runtime outside plugins/, stops Decky and retires detected standalone versions
(including renamed folders identified by plugin name). Their code is retained in
homebrew/ugm-options-migration/, outside Decky's discovery tree. Settings and
unrelated plugins are untouched. Master runtime files are root-owned with explicit
permissions. Decky is restarted only if previously active. A failed replacement
or restart rolls back the previous folders.

Linked/unknown legacy directories and modified masters are preserved with a warning.
The broker blocks modules whose standalone folders remain; Video Output also
checks output-manager, which migration never removes. Manual rollback: stop Decky,
remove the master and restore retired plugin folders, retaining settings/.
Building the ZIP alone never installs anything.

## Build / verify

```sh
python3 build.py
python3 -m unittest discover -s tests -v
npm ci --ignore-scripts --no-audit --no-fund
npm test
```

Runtime assembly uses Python's standard library only. The original frontend
bundles/native reader must already exist in adjacent module directories.
Dependencies above are development-only UI tests. Development ZIP in release/;
not a UGM DEB. Never changes packaging/release-2.8.5.json or its build_enabled flag.

To add a module: register its fixed file list/RPC methods/privilege in modules.json,
supply its compatible Decky bundle and engine beside this plugin, then rerun the
build and isolation/navigation tests. Core navigation is registry-driven.

## Verification limits

Local installation, real Steam/Decky registration and read-only RPCs to all four
modules have been verified. Automated checks do not certify controller navigation,
physical display switching, game launches or persistence across Steam sessions.
See the [current UGM guide](../../../docs/ugm-2.8.5/README.md). Credits/licenses:
NOTICE.md, LICENSE, modules/* and licenses/.
