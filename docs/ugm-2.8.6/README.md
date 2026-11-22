# Ubuntu Gaming Mode 2.8.6

**English** | [Español](../es/ugm-2.8.6/README.md) · [Documentation](../README.md)

Package **2.8.6-1**, Ubuntu **26.04 amd64**. Changes since 2.8.5:

- Support Steam client build **1794536281**, including its updated native updater API, while retaining support for **1792618285**.
- Fix Video Output commands on Ubuntu by using native Python subprocess credentials instead of a fixed `runuser` path.
- Skip saved Bluetooth controllers that are temporarily absent, avoiding repeated service failures while retaining their wake preferences.
- Replace the **Return to Gaming Mode** launcher icon with the cyberpunk UGM gamer-penguin logo.

The YouTube hook, Gamescope, VRS and other UGM Options panels are unchanged.
See [installation](../installation.md) and the [existing feature guide](../ugm-2.8.5/README.md) for usage.
