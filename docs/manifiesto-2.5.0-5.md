# Manifiesto UGM 2.5.0-5

Estado: versión preliminar para Ubuntu 26.04 amd64. Este
manifiesto describe componentes, no una validación física universal.

## En el paquete

- Gamescope 3.16.30 GBM, el mismo ejecutable de 2.0.0-1, con SHA256
  `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.
- Runtime `gamescope-session-plus`, sesión Steam DRM y herramientas de cambio
  Desktop/Gaming Mode, Doctor, setup, inventario y opciones HDR/VRR.
- Capa Vulkan VRS NVIDIA y AMD, cada una en 32/64 bits, con manifiestos,
  guardadas sin registro global en `/usr/lib/ubuntu-gaming-mode/vrs-bundled/`.
  El instalador guarda solo la ruta elegida; la sesión prepara las capas desde
  ese payload legible. La ruta APU RADV nativa no usa capa personalizada.
- App opcional de despertar por mando Bluetooth, icono, entrada de escritorio,
  política Polkit y servicio de reaplicación; instalada en el mismo `.deb` y
  desactivada por defecto en el menú.
- Icono principal de UGM, unidades systemd, autostart de inventario, scripts
  Debian de instalación/reconfiguración/rollback y documentación de usuario.

## Fuentes que deben quedar en Git

- `gamescope/versions/3.16.30-gbm/patches/`: serie 0001–0009 y `series`.
- `prototypes/{nvidia,amd}-vrs/`: `layer.c`, `spirv_policy.h`, `build.sh` y
  pruebas. Los `.so` y JSON se generan; no versionar binarios de prueba.
- `system/icons/ubuntu-gaming-mode.png` y `system/icons/ugm-bt-wake.png`.
- `src/`, `system/`, `packaging/`, `tests/`, README, SOURCE, changelogs y `docs/`.

El ejecutable Gamescope y los `.deb` permanecen fuera de Git (`.build/` y
`dist/`). La receta `packaging/build-deb.sh` comprueba el SHA256 del ejecutable
Gamescope y recompila ambas capas VRS. La prueba física del QAM y juegos aún
no está cerrada; consultar `docs/preparacion-publicacion-2.5.md`.
