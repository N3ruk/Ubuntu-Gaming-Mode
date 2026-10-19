# UGM Downsampling Sharpener — filtro 0.4 / Decky 0.2

Prototipo experimental de UGM 2.8. La versión 0.3 obtuvo una valoración positiva de fluidez y nitidez. La versión **0.4 conserva el resultado y optimiza exclusivamente la carga de muestras del shader**; instalada y verificada por SHA256 el 2026-10-14, sin backups. Decky permanece intacto en 0.2. Está integrado de forma opcional en 2.8.0-2.

Resultados e instalación actuales: [VALIDACION-0.4.md](VALIDACION-0.4.md). Histórico: [VALIDACION-0.3.md](VALIDACION-0.3.md), [VALIDACION-0.2.md](VALIDACION-0.2.md).

## Qué hace

- El parche de Gamescope detecta el juego enfocado cuando el *buffer que compone Gamescope* se reduce en ambos ejes (`scale.x/y > 1.001`). No puede detectar un escalado interno que el motor del juego o un upscaler hagan antes de entregar el frame.
- Una modificación del shader `cs_composite_blit.comp` aplica una nitidez tipo *unsharp* adaptativa al contraste, con margen de realce acotado alrededor del mínimo/máximo local y reutilización por mosaicos de las muestras con gestión de color, **después de muestrear la imagen reducida del juego y antes de mezclar Steam/QAM**. No toca el escalador ni el valor de nitidez FSR/NIS/SGSR.
- El plugin Decky independiente ofrece switch (activación automática solo cuando hay downsampling), intensidad manual 0–100 (25 por defecto) y estado. Sin plugin, el filtro queda desactivado. La intensidad guardada controla la ganancia y el margen de realce; el shader modula localmente la ganancia según el contraste. No modifica automáticamente ese valor guardado.
- Gamescope expone en cada raíz Xwayland `UGM_DOWNSAMPLE_SUPPORTED`, `UGM_DOWNSAMPLE_DETECTED`, `UGM_DOWNSAMPLE_ACTIVE`, y acepta `UGM_DOWNSAMPLE_ENABLED`/`UGM_DOWNSAMPLE_STRENGTH` como enteros de 32 bits. Decky reenvía las preferencias al reiniciarse la sesión.

## Construcción reproducible

Base: Gamescope de UGM `3.16.30-gbm`, commit `2dd5d3b0599eb2dda4dac5258aaec9374e1db16f`. No aplicar este parche a otra revisión sin revisarlo.

```bash
cd /ruta/al/gamescope-3.16.30-gbm
git apply --check /ruta/a/gamescope/versions/3.16.30-gbm/patches/0010-gamescope-downsample-sharpener.patch
git apply /ruta/a/gamescope/versions/3.16.30-gbm/patches/0010-gamescope-downsample-sharpener.patch
meson setup build-ugm-downsample --prefix=/usr/local --buildtype=debugoptimized
ninja -C build-ugm-downsample src/gamescope
```

Plugin:

```bash
cd prototypes/downsampling-sharpener/decky
pnpm install --frozen-lockfile
pnpm exec tsc --noEmit
pnpm build
```

`decky/` contiene `main.py`, `plugin.json`, `package.json` y `dist/index.js`. Al instalarlo en `~/homebrew/plugins/`, incluye `package.json`: Decky usa su campo `type: module` para cargar el frontend ES module. La compilación no se copia automáticamente. Ninguno de estos pasos modifica el Gamescope instalado.

## Plan de validación en hardware (histórico; completado parcialmente)

1. Ejecutar **esta build** de Gamescope en una sesión UGM de prueba e instalar el plugin Decky opcionalmente. Comprobar `UGM_DOWNSAMPLE_SUPPORTED=1` y estado «esperando» sin downsampling.
2. Juego a 3840×2160 con salida 2560×1440. Comprobar `DETECTED=1`; switch apagado debe dar `ACTIVE=0` y encendido con intensidad >0 debe dar `ACTIVE=1`. Repetir con salida igual a la del juego: debe volver a 0.
3. Capturar la misma escena a 0, 25, 50 y 100. Buscar detalle real, halos/aliasing, textos de Steam sin filtrar. Medir frametimes y confirmar que FSR/NIS/SGSR y el plugin Sharp Filter Selector no cambian de estado.
4. Repetir en SDR, HDR, VRR y con relación de aspecto distinta. La pasada fuerza composición cuando está activa y puede perder scanout directo; medir el coste antes de decidir integración en 2.8.

**Estado verificado:** 0.4 instalada y verificada por SHA256, SPIR-V validado y coincidente con el shader medido; 27 comparaciones FP32 idénticas a 0.3; menor coste GPU del shader en pruebas SDR/HDR; filtro apagado sin regresión apreciable. Decky sin cambios. **Observación de juego en HADES:** carga GPU 78–79 % sin filtro y 81–82 % con 0.4; nitidez y fluidez correctas. La comparación es observación de juego, no un benchmark de frametimes controlado. No convertir porcentajes del microbenchmark en porcentajes de carga total del juego.
