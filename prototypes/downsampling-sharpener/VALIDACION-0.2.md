# Nitidez downsampling 0.2 — 2026-10-13

## Estado real

Implementada, compilada y preparada para instalación manual. **No instalada por el asistente**. No había ningún Gamescope/juego activo durante esta intervención. La desaparición del tirón y la recuperación de los ~25 FPS comunicados por Carlos **no están verificadas**. No se han cambiado VRR, sincronización, limitadores, VRS ni preferencias del usuario.

## Diagnóstico

La versión 0.1 muestreaba cinco veces por píxel la capa del juego, incluyendo conversión de color, matriz, shaper y LUT 3D cuando están activos. Activa composición completa en DRM/Wayland/OpenVR: esto impide usar scanout directo aunque el escalado pudiera hacerlo. El camino DRM espera la composición antes de presentar (`vulkan_wait`). En `steamcompmgr.cpp`, con tearing y sin VRR, `WasCompositing()` cambia el flip de Async a Normal. Por tanto, un contador del juego >120 FPS no demuestra una presentación regular ni >120 fotogramas distintos en pantalla.

La reducción del coste del shader aborda un cuello de botella medido. No se ha quitado esa espera ni forzado tearing/VRR sin evidencia de la configuración del usuario. El coste básico de composición **sigue existiendo**. La capacidad VRR y que esté solicitada no prueban que esté activa: comprobar `GAMESCOPE_VRR_FEEDBACK`.

## Cambios

- Una única pasada; bloque 8×8 con caché compartida de 10×10 y halo en cruz, sin esquinas: 96 muestras para 64 píxeles, frente a 320. Se reutiliza la muestra ya transformada, también en HDR. 1.600 bytes de memoria compartida; sin imagen intermedia ni nueva sincronización CPU/GPU.
- Todos los hilos, incluidos los de relleno de los bloques de borde, llegan a la barrera. Borde de pantalla, rotación y overlays conservados.
- Ruta `colorspace_passthru`: acceso directo, porque los mosaicos no compensaron en esa ruta barata en RTX 2060.
- Intensidad espacial adaptativa: atenúa contraste casi plano y bordes fuertes; conserva el clamp al rango RGB local y evita filtrar a través del límite del viewport. Mismo control 0–100 y valor guardado. No se ha añadido historial temporal: la estabilidad en movimiento necesita prueba visual en juego.
- Decky comparte caché de descubrimiento durante diez segundos entre UI y sincronización; evita releer propiedades cuando no cambian. Lock para serializar operaciones. Usa credenciales nativas de `subprocess` en lugar de `preexec_fn`, que puede bloquearse al ejecutar desde hilos.

## Microbenchmark reproducible, no FPS de juego

RTX 2060, shader real de Gamescope mediante Vulkan, entrada sintética 3840×2160 → salida 2560×1440, intensidad 25, timestamps GPU, 160 muestras tras calentamiento por variante; rondas intercaladas. Sin bloqueo de relojes y con escritorio activo. LUTs identidad y datos sintéticos: no reproducen el perfil de color/carga de un juego concreto. Presentación/KMS/VRR no participan.

| Ruta | 0.1 mediana | 0.2 mediana | p95 0.1 → 0.2 |
|---|---:|---:|---:|
| SDR con gestión de color/LUT | 6,665 ms | 2,448 ms | 7,744 → 3,481 ms |
| PQ de entrada → salida SDR, LUT | 6,667 ms | 2,466 ms | 7,736 → 3,497 ms |
| Passthrough sin gestión de color | 0,377 ms | 0,385 ms | 0,384 → 0,405 ms |
| Filtro apagado, SDR compuesto | 1,378 ms | 1,379 ms | 1,962 → 1,851 ms |

Reducción del **63 % del tiempo del shader completo en la ruta SDR con LUT**. No equivale a 63 % más FPS ni a ahorro garantizado en otras GPU. En passthrough la adaptación de calidad añade ~0,009 ms (no se oculta esa regresión). El caso PQ no es una validación de salida HDR real.

Los logs completos están en `validation/results/final-*.log`. El loader imprimió `Authorization required, but no authorization protocol specified` al explorar plataformas de ventana; se mantuvo en los logs. El benchmark headless seleccionó la RTX 2060, completó Vulkan sin error y leyó los resultados. No hay capa Khronos de validación instalada; **no se afirma una pasada de esa capa**.

## Verificaciones

- Gamescope compila y `--version` funciona. Parche revisado con `git diff --check` y aplicación inversa `--check` sobre el árbol fuente.
- Tres shaders validados con `spirv-val --scalar-block-layout`.
- Variante de mosaicos sin adaptación: idéntica byte a byte al shader 0.1 en rutas ensayadas; tamaños 2560×1440 y 257×145 (bloques parciales), rotaciones 90/180/270°, capa overlay y viewport desplazado.
- Filtro apagado: variante final idéntica byte a byte a 0.1.
- Adaptación al 100 % en patrón sintético: cero violaciones del rango local o de la ganancia acotada (tolerancia de un nivel por cuantización). No sustituye valoración visual de calidad en movimiento.
- Seis pruebas del controlador; TypeScript y build Decky correctos.
- Instalador: preflight sin privilegios, instalación simulada con hashes, idempotencia y restauración manual/por fallo en árbol temporal.

## Instalación y vuelta atrás

```bash
/ruta/a/optimizar-nitidez-downsampling.sh --check
sudo /ruta/a/optimizar-nitidez-downsampling.sh
```

El payload está en `/ruta/a/ugm-nitidez-0.2.0/`. Sustituye seis archivos (Gamescope y plugin), comprueba hashes de origen y versión previa, guarda copia en `/var/backups/ugm-sharpener-0.2.0-*` y deja allí `restaurar.py`. Conserva preferencias y atributos de los archivos. No toca VRS, no reinicia servicios ni sesión. Si falla al instalar, restaura la copia anterior.

**Hay que salir y volver a entrar en modo gaming para ejecutar el Gamescope nuevo.** El proceso ya abierto sigue usando el binario anterior. Recargar únicamente el plugin desde Decky si no muestra versión 0.2.0. El script anterior `arreglar-decky-nitidez-downsampling.sh` pertenece a la corrección 0.1 y no debe usarse para esta actualización.

Restauración: `sudo /var/backups/ugm-sharpener-0.2.0-…/restaurar.py`, usando la ruta exacta que imprime el instalador, y reiniciar la sesión gaming manualmente.

## Prueba pendiente de fluidez

1. Misma escena/cámara y ajustes, una vez caliente. Anotar juego, resolución de render, salida/Hz, límite FPS y VRR. Ejecutar **sin sudo** `/ruta/a/diagnosticar-nitidez-downsampling.py` con el juego enfocado; informa también del hash del proceso, para detectar el binario antiguo aún abierto.
2. Medir 60 s de frametimes con filtro apagado y otros 60 s encendido a 25, con el QAM cerrado. Comparar mediana, p95/p99 y picos; no solo FPS medios. Repetir el par invirtiendo el orden.
3. Si sigue perdiendo fluidez, separar el coste de activar composición del coste del filtro con un tercer ensayo de composición forzada y filtro apagado. Solo temporalmente, restaurando el valor previo de `GAMESCOPE_COMPOSITE_FORCE` al terminar; no se deja forzado desde el instalador.
4. Verificar VRR efectivo y cadencia de presentación. No cambiar simultáneamente el limitador: invalidaría la comparación. Tras medir, ajustar la sincronización según Hz/rango VRR si procede.
5. Comparar 0/25/50/100 en texturas finas, vegetación en movimiento, texto dentro del juego, Steam/QAM, bandas negras y salida HDR real. La preferencia visual final aún está pendiente.
