# Port GBM de Gamescope 3.16.30

[English](README.md) | **Español** | [Auditoría técnica completa](AUDIT.txt)

Este directorio conserva el build reproducible de Gamescope utilizado para recuperar un scan-out 4K estable con NVIDIA en el sistema de referencia de Ubuntu Gaming Mode.

## Alcance

- Base upstream: Gamescope 3.16.30, commit `ad2763da1c48860f649abfe842a087188dcb6e20`.
- POC GBM original: commits `6d46d29..2bfc18c` de ValveSoftware/gamescope, originalmente sobre 3.16.25.
- Resultado del port: `b211c9d3eab1310eda2e511c965da3cf5910290e`.
- Árbol Git final: `b329fd0963e5ae797a14f4fe185f86c409937bc6`.
- Versión compilada: `3.16.30-8-gb211c9d`.
- SHA256 del binario validado: `96fbdd3f3c7e716e873cc66b79992f2b5e54d27c360ce4afb2c1bbb0628fffba`.

La funcionalidad es opt-in y UGM la activa mediante:

```bash
gamescope_drm_gbm_scanout=1
```

## Qué cambia

La ruta limpia asigna las imágenes de salida mediante Vulkan y las exporta como DMA-BUF. Este port permite que el backend DRM asigne buffers de scan-out mediante GBM y que Vulkan los importe. La ruta está diseñada para drivers como el propietario de NVIDIA, donde la colocación de memoria apta para scan-out puede diferir de una asignación de render normal.

No se modifica la implementación existente de Adaptive Sync/VRR. La ruta GBM corrige la asignación de los buffers de salida utilizados por el pipeline 4K + VRR existente.

## Qué no contiene

Este port no contiene el conector histórico Sharp Filter Selector/Decky/NIS de UGM 1.0.0-3. El código exacto del Gamescope 3.16.28 GOLD sigue sin estar disponible y no debe confundirse con este port reproducible de 3.16.30.

## Reproducción

Consulta [UPSTREAM.md](UPSTREAM.md) para la procedencia, [BUILD.md](BUILD.md) para la receta exacta y [VALIDATION.md](VALIDATION.md) para el alcance de aceptación.

Aplica los patches en el orden de `patches/series` sobre la base upstream exacta. Los ocho patches producen el árbol final `b329fd0963e5ae797a14f4fe185f86c409937bc6`.

El binario compilado no se guarda en Git. Debe distribuirse dentro de una release versionada de UGM e identificarse mediante su SHA256.
