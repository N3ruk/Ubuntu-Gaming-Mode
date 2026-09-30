# Código fuente y reproducibilidad de Ubuntu Gaming Mode

[English](SOURCE.md) | **Español**

Este repositorio contiene el código de **Ubuntu Gaming Mode 2.0.0-1** y el
registro reproducible del Gamescope incluido.

## Estructura

- `src/`: scripts de runtime y helpers de sesión.
- `system/`: unidades systemd, entradas de escritorio/sesión y configuración.
- `packaging/DEBIAN/`: metadatos y scripts de mantenimiento Debian.
- `packaging/build-deb.sh`: ensamblado reproducible del paquete.
- `gamescope/versions/3.16.30-gbm/`: patches, procedencia y validación.
- `tests/`: pruebas de las integraciones opcionales.
- `docs/`: instalación, rollback, arquitectura y rendimiento.

## Entradas binarias no guardadas en Git

El build requiere:

1. el ejecutable Gamescope validado;
2. el icono PNG de UGM.

```bash
GAMESCOPE_BIN=/ruta/al/gamescope \
UGM_ICON=/ruta/a/ubuntu-gaming-mode.png \
bash packaging/build-deb.sh
```

El script rechaza un Gamescope cuyo SHA256 no coincida con el autorizado para
la versión. Ambos binarios se incluyen en el `.deb` publicado, no en Git.

## Gamescope 3.16.30 GBM

- Base upstream: `ad2763da1c48860f649abfe842a087188dcb6e20`.
- Patches 0001–0008: port GBM derivado de upstream.
- Patch 0009: fallback de seguridad de UGM.
- Árbol final: `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.
- SHA256 del binario:
  `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.

La preparación exacta es:

```bash
git clone https://github.com/ValveSoftware/gamescope.git
cd gamescope
git checkout ad2763da1c48860f649abfe842a087188dcb6e20
git submodule update --init --recursive
git am /ruta/a/gamescope/versions/3.16.30-gbm/patches/*.patch
```

Consulta [`BUILD.md`](gamescope/versions/3.16.30-gbm/BUILD.md) para el toolchain
y [`VALIDATION.md`](gamescope/versions/3.16.30-gbm/VALIDATION.md) para el alcance
de las pruebas.

La cadena de versión embebida sigue siendo `3.16.30-8-gb211c9d` porque el
noveno cambio se compiló antes de convertir su diff en commit numerado. La
identidad del runtime se fija mediante el SHA256 y el árbol final anteriores.

## Nota histórica de 1.0.0-3

El Gamescope 3.16.28 de la release 1.0.0-3 no puede reconstruirse porque su
árbol modificado original se perdió. Esa limitación histórica no afecta a
2.0.0-1: el runtime 3.16.30 y sus nueve patches están documentados íntegramente.
