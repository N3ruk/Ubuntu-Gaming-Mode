# UGM 2.8.5-1 verification

**English** · [Resumen en español](../es/ugm-2.8.5/README.md#diagnóstico-y-límites-de-validación)

Target: **Ubuntu 26.04 amd64**, 2026-11-05. Compared against public **v2.8.3-2**.

## Automated checks

| Suite | Passed |
| --- | ---: |
| UGM integration, Doctor, lifecycle and legacy migration | 133 |
| YouTube / Steam updater hook contracts and compatibility | 41 |
| UGM Options worker isolation and assembly | 16 |
| UGM Options navigation and retained panels | 12 |
| Video Output Python engine | 66 |
| Video Output frontend | 5 |
| Non-Steam backend/catalog/VDF | 31 |
| Non-Steam Steam bridge and UI | 30 |
| **Total** | **334** |

Both TypeScript module typechecks pass. UI/Steam API fixtures are simulations,
not physical controller or launch tests. Expected injected failures in rollback
and worker tests are not failures of these suites.

## Package and source checks

- Build succeeds using an export containing only files selected for public Git;
  no local captures, generated master tree or node_modules are required as inputs.
  The recipe builds the native display reader and assembles the master itself.
- Two final assemblies using the same sources/toolchain and pinned external
  Gamescope binary produce byte-identical DEBs. This does not establish identical
  output across different compilers or distributions, or rebuild Gamescope itself.
- All **262** regular payload files match Debian md5sums; package members are
  root-owned and regular files/directories are not world-writable.
- Only UGM Options is distributed as a discoverable plugin: **166** runtime files.
  Its SHA256 manifest and original module hashes match. Native helper is executable.
- Gamescope SHA256 remains
  `4251c390803678e1d3ce626ae4c36b51c00763338efa358ef1d2d5f5c8ef7c03`.
  Existing VRS libraries and session runtime remain unchanged from 2.8.3-2.
- Version **2.8.5-1**, architecture **amd64**, Node.js >=22 dependency, maintainer
  syntax, package metadata and user-unit graph checked. Unit verification reports
  only an existing host spice-vdagent Install-section warning unrelated to UGM.
- APT simulation on Ubuntu 26.04 resolves an upgrade with no package removals.
  It does not perform an installation or establish a fresh-system session test.
- Public Markdown links checked; private captures, local paths, caches, secrets,
  sourcemaps and development dependencies are excluded from new published sources.

The release supplies the DEB and its matching SHA256SUMS, following the prior
release structure. Checksum files from previous releases must not be reused.

## Live evidence and limits

Before packaging, UGM Options was locally installed and registered in real
Steam/Decky; the four modules answered read-only RPCs. Earlier human testing
confirmed YouTube controller pause/resume and Steam update search. The new DEB
has **not** been installed on the host for this publication.

Pending: installed-DEB desktop/session lifecycle, real controller navigation,
physical display switching, Non-Steam launches/Proton/persistence across sessions,
and a full update to a genuinely newer Steam client. AMD/hardware and FPS claims
are not broadened by this release. Steam hooks remain restricted to their pinned
build/hash/API contracts; unknown clients are rejected, not silently supported.
