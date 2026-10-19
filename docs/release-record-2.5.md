# UGM 2.5.0-5 release record (historical)

**English** | [Español](preparacion-publicacion-2.5.md) · [Documentation index](README.md)

Source and package formed the stable `v2.5.0-5` GitHub release. Debian
versioning used `X.Y.Z-n`. This is a record of **that release**, not the
current 2.8 build procedure.

## Versioned layout at the time

- Code and packaging: `src/`, `system/`, `packaging/`, `tests/`.
- Resources: both `system/icons/` PNGs, desktop entries, Bluetooth policy and
  service. No icon depended on a file outside the repository.
- VRS: C source, headers, build scripts and tests under
  `prototypes/{nvidia,amd}-vrs/`, plus `prototypes/ugm-vrs-run` for lab use.
  Generated `.so` and JSON were not meant for Git.
- Gamescope: nine patches, `series`, provenance, hashes and validation under
  `gamescope/versions/3.16.30-gbm/`, without changing the 2.0 binary.
- Documentation: README, SOURCE, changelogs, licenses/notices and `docs/`.

## Historical build requirements

The 2.5 package needed Gamescope SHA256
`5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`,
32/64-bit C compilers, `dpkg-deb` and `desktop-file-validate`. The contemporary
script generated `dist/ubuntu-gaming-mode_2.5.0-5_amd64.deb` and eight VRS
payload entries. The current `packaging/build-deb.sh` is **2.8-only**; do not
run it with the 2.5 binary. Build outputs were ignored by Git. Changing any
package-included file required rebuilding the asset and updating its checksum.

## Validation at publication

61 automated tests passed, two builds were byte-identical, and the published
asset checksum was verified. Maintainer validation did not establish universal
compatibility for GPUs, drivers, games or controllers. Historical development
notes were kept locally under ignored `.build/source-archive/`.
