# UGM 2.8: downsampling con nitidez selectiva (parche 0010)

**Estado (14-10-2026): cerrado en el equipo de Carlos.** El filtro Gamescope 0.4 y el plugin Decky UGM Downsampling Sharpener 0.2 se probaron en HADES. Carlos confirma que la nitidez y la fluidez son correctas. Fuente 3840×2160, salida 2560×1440, HDR, VRR y FPS deslimitados: observó **78–79 % de carga GPU sin filtro y 81–82 % con filtro**. Son lecturas comunicadas por Carlos, no mediciones instrumentadas ni prueba de frametimes constantes. No se debe convertirlas en una reducción porcentual exacta del trabajo de toda la GPU.

## Disposición de fuentes

- [Parche 0010](../../../gamescope/versions/3.16.30-gbm/patches/0010-gamescope-downsample-sharpener.patch), inmediatamente después de 0009 en [series](../../../gamescope/versions/3.16.30-gbm/patches/series). Se aplica **una sola vez** sobre la base de los nueve parches; no se aplica encima de otro 0010. El parche 0010 se mantiene separado para que el histórico del binario estable 2.5 siga identificable.
- [Implementación y plugin](../../../prototypes/downsampling-sharpener/README.md); [validación de la versión 0.4](../../../prototypes/downsampling-sharpener/VALIDACION-0.4.md), [0.3](../../../prototypes/downsampling-sharpener/VALIDACION-0.3.md) y [0.2](../../../prototypes/downsampling-sharpener/VALIDACION-0.2.md). Los artefactos pesados de prueba quedan en `prototypes/downsampling-sharpener/validation/`; no forman parte del `.deb`.
- El segundo plugin de la integración opcional es **Sharp Filter Selector** (FSR/NIS/SGSR); no es parte del parche 0010 y el filtro downsampling no cambia sus ajustes.

## Cómo se consiguió

1. Se detecta downsampling de la superficie del juego enfocada **en ambos ejes**; no se infiere a partir de la resolución interna de un motor o de un upscaler. Gamescope expone a cada raíz Xwayland las propiedades de soporte, detección y actividad.
2. Decky ofrece activación e intensidad 0–100 (25 por defecto). Sin activar el complemento el filtro queda apagado. La preferencia se reenvía al iniciar una sesión Gamescope.
3. La nitidez adaptativa se aplica al juego **después del muestreo reducido y antes de componer Steam/QAM**, con ganancia modulada por contraste y límites de realce locales. No altera la nitidez propia de FSR/NIS/SGSR, ni la lógica de VRR, HDR o sincronización.
4. En 0.4 se repartieron cooperativamente las mismas 96 muestras por bloque de 8×8 píxeles; se sustituyeron ramas redundantes de carga, sin modificar las muestras usadas ni la fórmula. Se mantuvo la ruta con filtro apagado.

## Evidencia y límites

La comparación 0.3→0.4 dio **27 imágenes FP32 idénticas byte a byte** (incluyendo SDR/HDR sintético, intensidades, giros, bordes, overlays y tamaños impares). En microbenchmark Vulkan de RTX 2060, 3840×2160→2560×1440, intensidad 25, la mediana del shader SDR→HDR fue **2,456224 → 2,041728 ms (−16,88 %)**; el coste incremental frente al filtro apagado fue aprox. 1,077→0,663 ms. El benchmark mide el shader, no el porcentaje total de carga de un juego. [Resultados detallados](../../../prototypes/downsampling-sharpener/VALIDACION-0.4.md).

La versión 0.4 fue instalada y verificada por SHA256; no se crearon backups. Después, Carlos confirmó la experiencia en HADES y el sobrecoste de unos 2–4 puntos de carga GPU. El comportamiento en otros juegos/hardware no está probado por esa observación. Se deja esta función sin más cambios antes de retomar VRS.

## Build 2.8

El script de build sigue exigiendo un hash explícito de Gamescope por versión. El paquete 2.8.0-2 utiliza la serie 0001–0010 y el hash del binario 0.4 fijado en `build-deb.sh`; no reutiliza el binario 2.5. El binario medido figura en [la validación](../../../prototypes/downsampling-sharpener/VALIDACION-0.4.md).
