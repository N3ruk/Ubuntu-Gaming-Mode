# Prototypes and experiments

**English** | [Español](README_es.md) · [Documentation index](../docs/README.md)

This directory holds **source and test tools**, not a second installation
method. Some components are integrated in the 2.8 package; other files are
historical experiment records. Install the published `.deb` via the
[installation guide](../docs/installation.md), not scripts from a prototype.

| Component | Role in 2.8 | Start here |
| --- | --- | --- |
| `vrs-common/` | Shared conservative VRS policy, diagnostics and tests | [README](vrs-common/README.md) |
| `amd-vrs/`, `nvidia-vrs/` | Adapter sources compiled into optional 32/64-bit layers | [AMD](amd-vrs/README.md) · [NVIDIA](nvidia-vrs/README.md) |
| `downsampling-sharpener/` | Gamescope patch/plugin source and historical validation | [README](downsampling-sharpener/README_en.md) |
| `bluetooth-wake-*.py` | Development probes; not the installed app | [2.8 USB policy](../docs/ugm-2.8/usb-wake.md) |
| `ugm-vrs-run` | Local VRS experiment runner | [2.8 VRS](../docs/ugm-2.8/vrs.md) |

The package build is defined in `packaging/build-deb.sh`. Test results in
prototypes do not automatically establish whole-package or universal hardware
validation.
