# Port GBM de Gamescope 3.16.30

[English](README.md) | **Español** | [Auditoría técnica completa](AUDIT.txt)

Este directorio conserva el build reproducible de Gamescope utilizado para recuperar un scan-out 4K estable con NVIDIA en el sistema de referencia de Ubuntu Gaming Mode.

## Alcance

- Base upstream: Gamescope 3.16.30, commit `ad2763da1c48860f649abfe842a087188dcb6e20`.
- POC GBM original: commits `6d46d29..2bfc18c` de ValveSoftware/gamescope, originalmente sobre 3.16.25.
- Resultado del port GBM (patches 0001-0008): `b211c9d3eab1310eda2e511c965da3cf5910290e`.
- Resultado con protección (patch 0009): `190d2cf1edee4468c67ca30f2b55fd89289106fc`.
- Árbol Git final: `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.
- Versión compilada: `3.16.30-8-gb211c9d`.
- SHA256 del binario validado: `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.

La funcionalidad es opt-in y UGM la activa mediante:

```bash
gamescope_drm_gbm_scanout=1
```

## Qué cambia

La ruta limpia asigna las imágenes de salida mediante Vulkan y las exporta como DMA-BUF. Este port permite que el backend DRM asigne buffers de scan-out mediante GBM y que Vulkan los importe. La ruta está diseñada para drivers como el propietario de NVIDIA, donde la colocación de memoria apta para scan-out puede diferir de una asignación de render normal.

No se modifica la implementación existente de Adaptive Sync/VRR. La ruta GBM corrige la asignación de los buffers de salida utilizados por el pipeline 4K + VRR existente.

El patch 0009 se mantiene deliberadamente separado del port GBM upstream.
Gestiona un fallo real de `vkImportSemaphoreFdKHR` en la ruta de reescalado
preventivo: si no puede importar el semáforo timeline acquire o release,
Gamescope desactiva el reescalado preventivo para esa sesión y continúa por la
composición normal en lugar de desreferenciar un semáforo nulo. No cambia el
filtro elegido, VRR, HDR ni la ruta de scan-out GBM.

## Qué no contiene

Este port no contiene el conector histórico Sharp Filter Selector/Decky/NIS de UGM 1.0.0-3. El código exacto del Gamescope 3.16.28 GOLD sigue sin estar disponible y no debe confundirse con este port reproducible de 3.16.30.

## Reproducción

Consulta [UPSTREAM.md](UPSTREAM.md) para la procedencia, [BUILD.md](BUILD.md) para la receta exacta y [VALIDATION.md](VALIDATION.md) para el alcance de aceptación.

Aplica los patches en el orden de `patches/series` sobre la base upstream exacta. Los nueve patches producen el árbol final `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.

El binario validado sigue mostrando `3.16.30-8-gb211c9d` porque el patch 0009
se compiló antes de convertir su diff fuente en un commit y patch numerado. El
SHA256 anterior es la identidad canónica del runtime instalado con los nueve
patches.

El binario compilado no se guarda en Git. Debe distribuirse dentro de una release versionada de UGM e identificarse mediante su SHA256.
