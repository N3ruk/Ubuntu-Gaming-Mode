# Guía de desarrollo

[English](../development.md) | **Español** · [Índice de documentación](README.md)

## Cómo encaja el repositorio

UGM tiene una única receta de paquete Debian: [`packaging/build-deb.sh`](../../packaging/build-deb.sh).
Reúne los ejecutables de [`src/`](../../src/), la sesión, servicios y entradas
de escritorio de [`system/`](../../system/), el binario Gamescope versionado y
los payloads opcionales de VRS/Decky. [`packaging/DEBIAN/`](../../packaging/DEBIAN/)
contiene los metadatos y scripts de mantenimiento. [`tests/`](../../tests/)
tiene pruebas Python del proyecto; [`prototypes/`](../../prototypes/README_es.md)
contiene código, fixtures y validaciones independientes por componente.
[`gamescope/`](../../gamescope/README_es.md) registra los parches y su
procedencia upstream. El binario y el `.deb` son salidas de build, no fuentes
versionadas en Git.

La [arquitectura](arquitectura.md) explica el flujo Desktop/Gaming Mode
instalado. El [resumen 2.8](ugm-2.8/README.md) enlaza las funciones y sus
pruebas. La [guía de fuentes](../../SOURCE_es.md) recoge procedencia y licencias.

## Comprobaciones locales

Desde la raíz:

```bash
python3 -m unittest discover -s tests
bash prototypes/vrs-common/tests/run.sh
```

El runner VRS necesita compilador, herramientas de desarrollo Vulkan y
dependencias de build de 32 bits. Los runners GPU de
`prototypes/{amd,nvidia}-vrs/tests/` necesitan el hardware respectivo; una
prueba omitida o simulada no equivale a validación física. Las pruebas Python
no instalan ni reconfiguran UGM.

## Construir un paquete Debian

El script toma la versión de `packaging/DEBIAN/control`, exige un
`GAMESCOPE_BIN` explícito, comprueba su SHA256 según la versión y escribe el
paquete en `dist/`. El binario 2.8 incluye el parche 0010; el hash histórico
Gamescope 2.5 no sirve. Consulta la [receta Gamescope](../../gamescope/versions/3.16.30-gbm/BUILD.md)
y [downsampling](ugm-2.8/downsampling.md). El script también compila cuatro
bibliotecas VRS y valida el paquete. Ejecútalo solo con las dependencias de
build y un binario Gamescope 2.8 verificado:

```bash
GAMESCOPE_BIN=/ruta/absoluta/al/gamescope-verificado bash packaging/build-deb.sh
```

El script recrea su área de staging `.build/ubuntu-gaming-mode_<version>_<arch>`
y el paquete de `dist/`. No dirijas `WORKDIR` ni `OUTDIR` a datos ajenos. El
`SHA256SUMS` de la raíz corresponde a la **release publicada**, no a builds
nuevas arbitrarias. Si cambia un archivo incluido en el paquete, una release
futura necesita un paquete y checksum nuevos; no reutilices silenciosamente
el checksum anterior.

## Documentación y publicación

- Enlaza cada página inglesa con su equivalente español. Coloca el uso vigente
  en `docs/` / `docs/es/` y especifica la versión en registros históricos.
- Actualiza el README y los comandos de instalación al cambiar de paquete
  publicado. No presentes mediciones de prototipos como validación total.
- Antes de publicar, ejecuta pruebas, revisa enlaces Markdown e inspecciona
  payload, versión, dependencias y SHA256 del `.deb`. Los assets de una release
  son en la práctica una instantánea; cambiar `main` no cambia los paquetes ya
  descargados.
- No publiques capturas locales, directorios de build, datos de usuario ni
  credenciales.
