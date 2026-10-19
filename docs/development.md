# Developer guide

**English** | [Español](es/desarrollo.md) · [Documentation index](README.md)

## How the repository fits together

UGM has one Debian package build: [`packaging/build-deb.sh`](../packaging/build-deb.sh).
It assembles [`src/`](../src/) executables, [`system/`](../system/) session,
service and desktop files, the versioned Gamescope binary, and optional VRS/
Decky payloads. [`packaging/DEBIAN/`](../packaging/DEBIAN/) contains package
metadata and maintainer scripts. [`tests/`](../tests/) has project-level Python
tests; [`prototypes/`](../prototypes/README.md) contains component code,
fixtures and separate validation. [`gamescope/`](../gamescope/README.md)
records the patch series and upstream provenance. The binary and `.deb` are
build outputs, not Git source.

The [architecture](architecture.md) explains the installed desktop/Gaming Mode
flow. The [2.8 feature overview](ugm-2.8/README.md) links component behavior
and evidence. The [source guide](../SOURCE.md) records provenance and licensing.

## Local checks

From the repository root:

```bash
python3 -m unittest discover -s tests
bash prototypes/vrs-common/tests/run.sh
```

The VRS test runner needs a suitable compiler, Vulkan development tools and
32-bit build dependencies. GPU runners in `prototypes/{amd,nvidia}-vrs/tests/`
require their respective hardware; do not equate a skipped or simulated case
with physical validation. Python tests do not install or reconfigure UGM.

## Building a Debian package

The build script reads the version from `packaging/DEBIAN/control`, requires an
explicit `GAMESCOPE_BIN`, checks its version-specific SHA256, and writes the
package under `dist/`. The 2.8 binary includes patch 0010; the historical
2.5 Gamescope hash is not valid for it. See the [Gamescope build recipe](../gamescope/versions/3.16.30-gbm/BUILD.md)
and [downsampling documentation](ugm-2.8/downsampling.md). The script also
compiles four VRS libraries and validates package files. Run it only with the
required build dependencies and a verified 2.8 Gamescope binary:

```bash
GAMESCOPE_BIN=/absolute/path/to/verified/gamescope bash packaging/build-deb.sh
```

The script recreates its `.build/ubuntu-gaming-mode_<version>_<arch>` staging
area and `dist/` package. Do not point `WORKDIR` or `OUTDIR` at unrelated data.
The root `SHA256SUMS` is a **published-release artifact**, not a checksum of
arbitrary new builds. If package-included files change, a future release
requires a new package and checksum; do not silently reuse the old checksum.

## Documentation and release hygiene

- Keep English and Spanish pages linked to one another. Put current usage in
  `docs/` / `docs/es/`; state the version explicitly in historical records.
- Update the README and installation commands whenever the published package
  changes. Do not describe prototype measurements as whole-package validation.
- Before publishing, run tests, check Markdown links and inspect the `.deb`
  payload, version, dependency metadata and matching SHA256. A release asset
  is an immutable snapshot in practice; documentation-only changes on `main`
  do not change previously downloaded packages.
- Do not commit local captures, build directories, user data or credentials.
