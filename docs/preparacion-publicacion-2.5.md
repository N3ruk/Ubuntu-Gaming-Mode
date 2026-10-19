# Publicación de UGM 2.5.0-5

[English](release-record-2.5.md) | **Español** · [Índice](es/README.md)

> Registro histórico de 2.5. La receta de build actual es solo para 2.8.


Código fuente y paquete forman la release estable `v2.5.0-5` en GitHub.
La numeración Debian conserva el formato `X.Y.Z-n`.

## Estructura versionada

- Código y empaquetado: `src/`, `system/`, `packaging/`, `tests/`.
- Recursos: los dos PNG de `system/icons/`, sus entradas desktop y la política
  y servicio Bluetooth. Ambos iconos están dentro del repositorio, en
  `system/icons/`; no dependen de archivos externos al árbol.
- VRS: fuentes C, cabeceras, build scripts y pruebas de `prototypes/nvidia-vrs/`
  y `prototypes/amd-vrs/`, además de `prototypes/ugm-vrs-run` como herramienta
  de laboratorio. No subir los `.so`/JSON generados en esos directorios.
- Gamescope: los nueve patches, `series`, procedencia, hashes y validación en
  `gamescope/versions/3.16.30-gbm/` (sin cambiar el binario de 2.0).
- Documentación: README, SOURCE, changelogs, licencias/notices y `docs/`.

## Construcción del artefacto exacto

Necesita el Gamescope validado (SHA256
`5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`),
compilador C de 64 y 32 bits, `dpkg-deb` y `desktop-file-validate`.
Desde la raíz del proyecto:

```bash
GAMESCOPE_BIN=/ruta/al/gamescope-validado bash packaging/build-deb.sh
(cd dist && sha256sum -c ../SHA256SUMS)
python3 -m unittest discover -s tests -v
```

El script genera `dist/ubuntu-gaming-mode_2.5.0-5_amd64.deb` y las ocho
entradas VRS en el payload. `dist/` y `.build/` están ignorados por Git. Si
cambia un archivo que el `.deb` incluye (README, SOURCE, changelog, fuentes,
iconos), **reconstruir y actualizar SHA256SUMS** antes de distribuirlo.

## Estado de validación

La versión se publica como estable tras la validación del mantenedor. Se
superaron 61 pruebas automatizadas, se obtuvieron dos builds idénticas byte a
byte y se comprobó el checksum del asset publicado. Esto no implica
compatibilidad universal con cualquier GPU, controlador, juego o mando.

Las notas personales de desarrollo y prototipos anteriores están conservadas
localmente en `.build/source-archive/`, ignorado por Git.
