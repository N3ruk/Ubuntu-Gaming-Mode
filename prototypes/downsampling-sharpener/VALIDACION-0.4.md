# Nitidez downsampling 0.4 — menos coste, misma imagen

[English](VALIDATION-0.4.md) | **Español** · [Componente](README.md)

> Los resultados bajo `validation/` son artefactos locales ignorados por Git;
> las rutas citadas abajo no son descargas disponibles en GitHub.

## Estado y objetivo

2026-10-14. **Instalada y verificada por SHA256; sin backups.** Pruebas realizadas el 2026-10-13. Los 50 archivos protegidos de plugins siguen intactos. `validation/v0.4/installation-result.json` (Verificación de instalación).

La evaluación de la versión 0.3 indicó fluidez y nitidez adecuadas. Observación comunicada: HADES, fuente 3840×2160, salida 2560×1440, HDR y VRR, FPS deslimitados: carga GPU aproximada 78 % sin filtro y 88 % con él. Son observaciones del usuario, no medidas instrumentadas de laboratorio. No se dispone de una captura de la sesión de juego que identifique su colorspace, LUT, FPS y frametimes exactos.

Objetivo: reducir trabajo GPU sin alterar la imagen ni la presentación. No se modifica Decky, su detección ni su intensidad guardada.

## Cambio exclusivamente en el shader del parche 10

Se distribuyen cooperativamente las **mismas 96 muestras** por bloque de 64 píxeles. Un bucle de muestreo común sustituye las cuatro llamadas dispersas para los bordes y la llamada central. Cada posición de la caché recibe exactamente la misma muestra color-managed; los cuatro rincones, no utilizados, se omiten.

- Mismos bloques 8×8, caché `vec4[100]`, precisión y barrera.
- Misma curva, intensidad, ganancia, límites de overshoot y protección de letterbox.
- Mismo orden de composición y overlays; no se alteran VRR, sincronización, cadencia, escalado, HDR ni gestión de color.
- Filtro apagado y ruta passthrough no usan el bucle nuevo.
- No introduce extensiones, dependencias de subgrupos ni requisitos específicos de NVIDIA.
- La necesidad de composición con el filtro activo sigue existiendo; no se puede prometer eliminar todo el sobrecoste manteniendo este filtro.

`validation/v0.4/v0.3-to-v0.4.diff` (Diff exclusivo 0.3→0.4).

Los dos parches completos son idénticos y siguen siendo el parche experimental 0010 sobre el commit base `2dd5d3b0599eb2dda4dac5258aaec9374e1db16f`. No aplicar 0010 encima de otra versión del propio 0010. El diff enlazado permite identificar el ajuste de shader entre 0.3 y 0.4.

## Rendimiento

RTX 2060; microbenchmark Vulkan sin ventana, fuente sintética 3840×2160, salida 2560×1440; intensidad 25; cinco rondas intercaladas, primera descartada, 160 timestamps por shader. Relojes sin fijar. **Mide compute del shader, no FPS, porcentaje total de GPU ni frametimes de presentación del juego.**

| Ruta sintética | 0.3, mediana ms | 0.4, mediana ms | Reducción |
|---|---:|---:|---:|
| sdr | 2.431712 | 2.026784 | 16.65 % |
| sdr-to-hdr | 2.456224 | 2.041728 | 16.88 % |
| linear-to-hdr | 2.472672 | 2.126720 | 13.99 % |
| scrgb-to-hdr | 2.380704 | 2.015456 | 15.34 % |
| pq-to-hdr | 2.906112 | 2.129440 | 26.73 % |
| passthrough | 0.393952 | 0.393504 | 0.11 % |
| off-sdr | 1.378912 | 1.379040 | -0.01 % |
| off-hdr | 1.379136 | 1.379072 | 0.00 % |
| sdr-to-hdr-repeat | 2.457088 | 2.041600 | 16.91 % |

En SDR→HDR: **2,456224 → 2,041728 ms (−16,88 %)**. Repetición en orden inverso: 2,457088 → 2,041600 ms (−16,91 %). p95: 2,458112 → 2,043424 ms en la primera de estas mediciones.

Tomando el filtro apagado como referencia dentro del mismo microbenchmark: el coste incremental pasa aproximadamente de **1,077 a 0,663 ms (−38,5 %)**. Esto no significa −38,5 % en la carga total de HADES, ni permite convertir directamente el 88 % observado en un porcentaje final.

La primera ronda SDR tuvo un p95 mayor en el candidato (2,515776 frente a 2,433056 ms); se conserva el resultado. La repetición sin compilación concurrente y con el SPIR-V extraído del binario construido dio mediana **2,438496 → 2,030592 ms** y p95 **2,447040 → 2,045728 ms**. No se atribuye la variación a una causa no comprobada.

El escenario PQ con bilinear manual es una prueba sintética adicional, no una identificación de la ruta real de HADES. El harness usa LUTs de prueba, no las del monitor. Las mediciones de rendimiento usan destino RGBA8; para detectar diferencias ocultas por cuantización se comparó por separado el cálculo con RGBA32F. No equivale a validar salida física HDR de 10 bits ni KMS/VRR.

Logs y datos: `validation/v0.4/performance-results.json` (performance-results.json), `validation/v0.4/sdr-repeat.log` (repetición SDR). Los avisos `Authorization required, but no authorization protocol specified` del loader al explorar plataformas aparecen en los logs; las pruebas headless terminaron con código 0 en RTX 2060.

## Igualdad y construcción

- **27 casos FP32 idénticos byte a byte y sin NaN/Inf**: colorspaces 0–4, salida SDR/PQ, scRGB hasta 1000 nits en datos de prueba, alpha variable, intensidades 0/1/25/50/100, rotaciones 90/180/270°, letterbox, overlay, bilinear, inverse tone mapping y tamaños impares/menores que un bloque, incluido 1×1.
- Los 9 casos de rendimiento y 4 comparaciones exploratorias/repetidas también dan imágenes idénticas a 8 bits. Datos brutos duplicados eliminados después de registrar igualdad y SHA256.
- `spirv-val --scalar-block-layout` correcto. El SPIR-V extraído del header realmente compilado es **idéntico al shader medido**.
- Gamescope compilado y `--version` correcto.
- Parche comprobado en repositorio temporal aislado: aplicable sobre la base, coincide con los nueve archivos del árbol y revierte exactamente; `git diff --check` correcto.
- 51 archivos protegidos comprobados antes de instalar: Gamescope actual y fuentes/instalación de plugins. No se escribe en Decky.
- El harness amplía el existente con EOTF de salida, ITM, bilinear y salida/entrada FP32. No hay capa Khronos de validación Vulkan instalada; la validación citada de SPIR-V no equivale a validación de toda la API Vulkan.

`validation/v0.4/quality-results.json` (Casos de calidad), `validation/v0.4/validate_quality.py` (verificador reproducible), `validation/v0.4/validate_performance.py` (benchmark reproducible), `validation/v0.4/integration-checks.json` (verificación de integración).

## Instalación sin backups

Payload: `.build/downsampling-sharpener/releases/0.4.0/0-gamescope`.

SHA256: `4251c390803678e1d3ce626ae4c36b51c00763338efa358ef1d2d5f5c8ef7c03`.

```bash
cd /ruta/a/Ubuntu-Gaming-Mode
prototypes/downsampling-sharpener/validation/v0.4/install-local-0.4.0.sh --check
sudo prototypes/downsampling-sharpener/validation/v0.4/install-local-0.4.0.sh
```

Solo sustituye `/usr/lib/ubuntu-gaming-mode/gamescope`, atómicamente, comprobando hashes y conservando propietario, permisos y atributos. Rechaza instalación si Gamescope está ejecutándose o el destino cambió. No crea backups ni reinicia servicios; volver a UGM 2.5 se haría reinstalando el DEB publicado en Git.

**Seguimiento en el juego:** Se valoraron positivamente la nitidez y la fluidez, y se comunicaron las lecturas de carga GPU recogidas abajo. No hay registro instrumentado de FPS/frametimes ni escena controlada; esas magnitudes siguen sin medirse.

## Observación posterior en HADES

Tras instalar 0.4, la evaluación de juego indicó que se mantienen la fluidez y la nitidez. Con la
misma configuración descrita arriba, se comunicaron **78–79 % de carga GPU sin filtro
y 81–82 % con filtro**. Es una observación práctica con FPS deslimitados, no
una medición instrumentada de frametimes ni una conversión directa de los
resultados del microbenchmark a carga total de GPU. Se cierra el apartado de
downsampling sin más cambios antes de continuar con VRS.
