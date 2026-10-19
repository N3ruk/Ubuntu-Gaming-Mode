# UGM-ICONS bundled source

`UGM-ICONS.tar.xz` is a reproducible archive of the `UGM-ICONS/` directory
from Carlos's supplied `UGM-ICONS.zip` (SHA256:
`2139276a6188f20cb3f5bd1652f65af6c0db7191fad38aae7537b628b94940a4`).
The bundled archive SHA256 is
`151edb27017b68e8f04503cbac3c1eda6f16e3e397d07063d34a77617311fc49`.
The ZIP's separate installation script is not run by UGM. UGM uses Debconf and
`ugm-icon-theme` instead. The archive retains `AUTHORS` and `COPYING` (GPL-3.0)
and the icon symlinks. The theme is optional at install time.

To regenerate from the supplied ZIP, extract only `UGM-ICONS/` with `unzip`,
then run:

```sh
tar -C extracted --sort=name --mtime='@1792368000' --owner=0 --group=0 --numeric-owner -cJf UGM-ICONS.tar.xz UGM-ICONS
```
