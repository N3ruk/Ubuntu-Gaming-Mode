# Third-party notices and adaptation

UGM Añadir contenido Non-Steam is an independent UGM adaptation by N3ruk,
not an official Valve, Decky or upstream-author product.

## Add Non-Steam Games — BSD-3-Clause

https://github.com/danielcamilo1/decky-add-non-steam-games
Revision: 7f77800b715f5b17efc6a394c9edeb8be6a58841 (1.7.13).
Copyright (c) 2026, Daniel Vergara.
Original Copyright (c) 2022–2024, Steam Deck Homebrew.
The full original license is retained in LICENSE.

Application discovery, desktop field processing and shortcut creation sequencing
were adapted and rewritten for UGM. Artwork, credentials, collections, background
scanning, original UI and automatic Steam restart are not included.

src/compatTools.ts is retained from PR #1 by ReemX, pinned to
edbe170bb0880b11dc36d15666e1a4f7611a0c6b (not assumed merged or validated):
https://github.com/danielcamilo1/decky-add-non-steam-games/pull/1
The upstream BSD license applies. The response method/field discovery credits
FlashyReese/decky-wine-cellar (MIT), as preserved in the file's comment:
https://github.com/FlashyReese/decky-wine-cellar
No code is copied directly from Wine Cellar in this adaptation.

## SDK / tooling

@decky/api, @decky/ui, @decky/rollup: Steam Deck Homebrew, BSD-3-Clause.
React and TypeScript development tools retain their upstream licenses in npm.
The frontend uses Decky's host-provided React/API/UI, not a bundled React copy.
Python backend requires only the standard library. Build/test dependencies are
not included in the runtime ZIP. Versions and integrity are pinned in package-lock.json.
