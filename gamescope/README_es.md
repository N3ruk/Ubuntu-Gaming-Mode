# Gamescope incluido en Ubuntu Gaming Mode

[English](README.md) | **Español**

Ubuntu Gaming Mode 2.0.0-1 incluye un Gamescope 3.16.30 GBM reproducible:

```text
versión:  gamescope 3.16.30-8-gb211c9d
SHA256:   5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256
ruta:     /usr/lib/ubuntu-gaming-mode/gamescope
```

El binario se distribuye dentro del `.deb` y no se guarda en Git.

## Fuentes reproducibles

[`versions/3.16.30-gbm/`](versions/3.16.30-gbm/README_es.md) contiene:

- commit upstream base `ad2763da1c48860f649abfe842a087188dcb6e20`;
- ocho patches del port GBM derivado de upstream;
- un noveno patch de fallback seguro de UGM;
- orden exacto de aplicación;
- árbol fuente final `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`;
- receta de build, toolchain, auditoría y validación.

UGM activa la ruta GBM mediante `gamescope_drm_gbm_scanout=1`. Esta ruta permite
que el backend DRM asigne buffers de scan-out con GBM y que Vulkan los importe,
evitando la corrupción 4K observada anteriormente en el sistema NVIDIA de
referencia. El patch 0009 impide que un fallo de importación del semáforo de
reescalado derribe la sesión y continúa mediante composición normal.

## Alcance

La serie no selecciona filtros de escalado ni modifica HDR o VRR. La integración
con Sharp Filter Selector utiliza las capacidades expuestas por Gamescope/Steam,
no un conector oculto dentro de esta serie.

El Gamescope 3.16.28 histórico de UGM 1.0.0-3 no era reproducible porque su árbol
modificado se perdió. Esta limitación queda restringida a aquella release; el
runtime 3.16.30 de 2.0.0-1 está documentado por completo.
