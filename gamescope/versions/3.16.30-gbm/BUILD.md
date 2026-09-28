# Exact build recipe

## Source preparation

```bash
git clone https://github.com/ValveSoftware/gamescope.git
cd gamescope
git checkout ad2763da1c48860f649abfe842a087188dcb6e20
git submodule update --init --recursive
git am /path/to/patches/*.patch
```

After applying the series, verify:

```bash
git show -s --format=%T HEAD
```

Expected tree:

```text
5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1
```

## Build and tests

```bash
meson setup build --prefix=/usr/local --buildtype=release
ninja -C build
meson test -C build --print-errorlogs
```

Observed toolchain:

| Component | Version/value |
|---|---|
| Meson | 1.10.1 |
| Ninja | 1.13.2 |
| GCC/G++ | Ubuntu 15.2.0-16ubuntu1 |
| GBM | 26.0.8-1ubuntu0.3 |
| Optimization | `-O3` |
| LTO | disabled |
| Strip | disabled |
| OpenVR | enabled |
| PipeWire | auto/found |
| GBM | `HAVE_GBM=1` |

Test result recorded for the eight-patch GBM baseline:

```text
Ok:   68
Fail: 0
```

Validated executable after compiling patch 0009:

```text
version: gamescope 3.16.30-8-gb211c9d (gcc 15.2.0)
size:    5158080 bytes
sha256:  5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256
```

Patch 0009 was compiled with `meson compile -C build gamescope`; no new Meson
test-suite run was recorded after that single-file safety change. The installed
runtime was then validated against the real import failure. Its embedded version
string remains `-8-gb211c9d` because the source diff was committed only after
the validated binary had been built.

Exact binary identity depends on the recorded toolchain and dependencies. Source reproducibility is anchored by the final Git tree; release binary integrity is anchored by SHA256.
