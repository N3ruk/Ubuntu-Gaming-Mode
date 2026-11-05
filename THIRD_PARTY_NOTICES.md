# Third-party notices

[Español](THIRD_PARTY_NOTICES_es.md)

Ubuntu Gaming Mode contains or derives from third-party software. The MIT
license in the repository root applies to original UGM project code only.
Third-party software remains under its respective copyright and license terms.

## ChimeraOS gamescope-session

Parts of the UGM session runtime are based on or derived from
[ChimeraOS/gamescope-session](https://github.com/ChimeraOS/gamescope-session).

Upstream license: MIT.

Copyright © 2020-2023 Joaquín Ignacio Aramendía

The complete upstream license text is preserved in:

- [licenses/CHIMERAOS-GAMESCOPE-SESSION-LICENSE](licenses/CHIMERAOS-GAMESCOPE-SESSION-LICENSE)

## Valve Gamescope

UGM 2.0.0-1, 2.5.0-5 and 2.8.0-2 distribute a modified Gamescope binary based on
[ValveSoftware/gamescope](https://github.com/ValveSoftware/gamescope).

The main Gamescope project is distributed under the BSD 2-Clause License and
its upstream LICENSE also carries notices for bundled/derived components such
as OpenColorIO tetrahedral 3D LUT code and the ReShade effects compiler.

Copyright (c) 2013-2022, Valve Corporation  
Copyright (c) 2022, NVIDIA CORPORATION  
All rights reserved.

The complete upstream license and bundled third-party notices are preserved in:

- [licenses/GAMESCOPE-LICENSE](licenses/GAMESCOPE-LICENSE)

The Gamescope binary bundled with UGM is a custom build documented by version.
Its upstream base, complete patch series, final tree and build record are
published under `gamescope/versions/3.16.30-gbm/`; see
[gamescope/README.md](gamescope/README.md).

## Sharp Filter Selector

UGM 2.8.0-2 optionally bundles
[Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector)
under the BSD-3-Clause license retained in
[integrations/decky/sharp-filter-selector/LICENSE](integrations/decky/sharp-filter-selector/LICENSE),
and UGM Downsampling Sharpener. The plugins are installed only when selected;
Decky Loader is fetched separately by its official installer.

## Trademarks and project names

Steam, SteamOS, Steam Deck and Gamescope are associated with Valve Corporation.
NVIDIA and GeForce are associated with NVIDIA Corporation. Ubuntu is associated
with Canonical Ltd.

Ubuntu Gaming Mode is an independent community project and is not affiliated
with or endorsed by Valve, NVIDIA, Canonical, Bazzite or ChimeraOS.

## UGM-ICONS theme

The optional UGM-ICONS theme comes from the archive supplied by N3ruk for
this version. Its `COPYING` file states GPL-3.0; `COPYING` and `AUTHORS` are
kept in the package and inside the bundled theme archive. Third-party-derived
icons retain their respective rights.

## UGM Video Output (2.8.5)

[UGM Video Output](integrations/decky/ugm-video-output/README.md) adapts N3ruk's
Video Output ALLY for DOCKs, derived from Output Manager by joeatethebeans.
BSD-3-Clause notices are retained in the component's LICENSE and NOTICE.md.
Exact source provenance is in ORIGIN.json; frontend dependency licenses and
Valve's MIT gamescope_control protocol license are shipped in its licenses/.

## UGM Options (2.8.5)

UGM Options bundles the four original engines and frontend panels under modules/,
retaining all original licenses and attribution. Standalone source folders remain
assembly inputs; only the master is offered by the installer. See
[integrations/decky/ugm-options/NOTICE.md](integrations/decky/ugm-options/NOTICE.md).

## Non-Steam (2.8.5)

[UGM Non-Steam](integrations/decky/ugm-non-steam/README.md) is adapted from
Daniel Vergara / Steam Deck Homebrew's Add Non-Steam Games, including ReemX's
compatibility-tool work. BSD-3-Clause notices and pinned provenance are retained
in its LICENSE, NOTICE.md and ORIGIN.json. It ships inside UGM Options, not as a
second installed plugin.
