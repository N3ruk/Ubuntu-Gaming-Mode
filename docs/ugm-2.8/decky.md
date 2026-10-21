# Optional Decky integration in UGM 2.8

**English** | [Español](../es/ugm-2.8/decky.md) · [2.8 overview](README.md)

The package asks **separately**, defaulting to no, whether to install Decky
Loader, Sharp Filter Selector and UGM Downsampling Sharpener. If Decky is
already present, either plugin can be selected independently. These questions
run during installation or reconfiguration, not when browsing the source.

**Sharp Filter Selector also enables SGSR from QAM.** When Sharp is selected
and the active Gamescope supports SGSR, the plugin translates Steam's FSR
selection into Gamescope's SGSR filter. Gamescope runs the algorithm, but
installing Gamescope alone does not link that QAM choice to SGSR. The plugin
also lets you choose NIS or force FSR. With HDR input, Gamescope uses FSR
instead of SGSR. UGM leaves an existing plugin version untouched, so this
behavior depends on that version having SGSR integration.

The two plugin payloads under [`integrations/decky/`](../../integrations/decky/)
contain only `plugin.json`, `package.json`, `main.py`, `dist/index.js` and the
BSD-3-Clause license for Sharp Filter Selector. They derive from the tested
Sharp Filter Selector 1.1.0 and UGM Downsampling Sharpener 0.2.0. They contain
no user settings, logs, `node_modules`, sourcemaps or test files. Sources and
history for the downsampling plugin remain under
[`prototypes/downsampling-sharpener/`](../../prototypes/downsampling-sharpener/).

[`ugm-decky-integration`](../../src/ugm-decky-integration) leaves an existing
plugin directory untouched. Otherwise it stages the payload and atomically
publishes it under `~/homebrew/plugins/` for the UGM user. Decky settings in
other directories are outside UGM package management and are not removed when
UGM is uninstalled. This option does not change VRS or the installed Gamescope.

If Decky Loader is absent, the helper downloads SteamDeckHomebrew's **official**
CLI installer at revision `9b4e29d2a33b969da11114cce18422a87425f60e` and
verifies SHA256 `2403ddbfec8ab320c33729bcb1c8176d74952de71f731ba0c71ded711058be2d`
before running it. The official installer downloads a Decky release at that
time: it needs network access and that release is not pinned in UGM's `.deb`.
2.8.0-2 was tested with the Decky version available then; future installations
depend on the upstream release available when installed. An optional installer
failure warns but does not abort UGM; retry with
`sudo dpkg-reconfigure ubuntu-gaming-mode`. `curl` and `jq` are recommended
packages; if unavailable, the Decky option fails without blocking UGM.

**2.8.0-2 fix:** a clean 2.8.0-1 installation inherited `umask 077` from the
postinst script, making `PluginLoader` root:root `0700` so plugins could not
import their modules. The helper now runs the official installer with
`umask 022`, normalizes `PluginLoader` to `0755` (also when upgrading from
2.8.0-1), publishes plugin payloads with explicit permissions and reloads an
active service.
