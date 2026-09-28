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
b329fd0963e5ae797a14f4fe185f86c409937bc6
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

Expected test result:

```text
Ok:   68
Fail: 0
```

Expected executable:

```text
version: gamescope 3.16.30-8-gb211c9d (gcc 15.2.0)
size:    5158080 bytes
sha256:  96fbdd3f3c7e716e873cc66b79992f2b5e54d27c360ce4afb2c1bbb0628fffba
```

Exact binary identity depends on the recorded toolchain and dependencies. Source reproducibility is anchored by the final Git tree; release binary integrity is anchored by SHA256.
