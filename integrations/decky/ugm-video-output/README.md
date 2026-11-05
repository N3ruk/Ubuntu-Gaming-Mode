# UGM Video Output

Video-only Decky plugin for **Ubuntu Gaming Mode**. Version **1.0**.

- Shows connected screens by saved label or detected manufacturer/model (EDID).
- Selects an output from Gaming Mode, retaining the original guarded video engine.
- Internal panels use their detected name too: there is no fixed ASUS ALLY entry.
- No audio routing, extra settings pages, resolution or refresh-rate controls.

## Installation

UGM 2.8.5 includes this engine as the **Video Output** panel inside **UGM Options**,
not a separate Decky entry. Accept UGM Options in the installer with an existing
Decky Loader or its optional installation. The master migrates detected standalone
copies while preserving their settings. An unrelated output-manager is not removed
and prevents a second writer. [Installation and limits](../../../docs/ugm-2.8.5/README.md).

## Compatibility and behavior

Requires UGM's DRM Gamescope session, Python 3, systemd/logind, xprop (`x11-utils`),
libwayland-client and Decky Loader. UGM ships the read-only native helper under
`dist/ugm-active-display`; SteamOS gamescopectl and gamescope-environment are not
required. The compositor binary and startup wrapper are not modified by this
adaptation. The session owner's UID is discovered, not assumed to be 1000.

The original engine validates EDID + connector ID, checks its transient user.slice
scope/freezer and suspension epoch, serializes selection, nudges the compositor
and requires native active-connector plus DRM enabled confirmation. Failed
operations do not claim success; owned changes are rolled back/released using
the original rules. Unknown DRM force state, unverified EDID or cross-GPU switching
are rejected. The internal screen is never forced off. Actual support for a
particular GPU/driver/dock requires a real Gaming Mode test.

No default monitor is configured on a fresh install. Compatible saved preferences
are handled by the retained engine/API. The startup helper is retained for source
compatibility but UGM does not wire it into its boot sequence.

## Build and checks

```sh
npm ci --ignore-scripts --no-audit --no-fund
npm run typecheck
npm run build
bash native/build.sh
python3 -m unittest discover -s tests -v
node --test tests/frontend.test.mjs
python3 build.py --out ../../../.build/ugm-video-output
```

Native build requirements: C compiler, pkg-config, wayland-scanner and Wayland
client development headers. The DEB ships the built reader; users do not build it.
Source includes the pinned protocol and dependency lockfile. No host settings,
EDIDs, node_modules, sourcemaps or test files enter the DEB runtime payload.

Automated checks are simulated/isolated; they do not certify physical switching
on UGM hardware. See [release limits](../../../docs/ugm-2.8.5/README.md).

## Credits

See LICENSE, NOTICE.md, ORIGIN.json and licenses/. Original Output Manager credits
and BSD-3-Clause terms are preserved.
