# Nitidez downsampling 0.3 — ajuste exclusivo del shader

## Estado

**Instalada el 2026-10-13, sin crear backups**. SHA256 del destino comprobado después de la sustitución atómica. No había Gamescope activo; se usará al entrar en modo gaming. La evaluación posterior indicó que 0.3 mantiene la fluidez y ofrece la nitidez adecuada (HADES, 4K→1440p, HDR/VRR); la optimización posterior se documenta en VALIDACION-0.4.md.

**Decky no se ha modificado**, ni sus preferencias ni sus archivos fuente/instalados comprobados. Respecto al parche 0.2, solo cambia `src/shaders/cs_composite_blit.comp`. Sin cambios en detección, composición, sincronización, VRR, escalador ni VRS.

## Ajuste del parche 10

- Ganancia base: `0.8 * intensidad` → `2.4 * intensidad`.
- Atenuación máxima en bordes fuertes: 50 % → 35 %. Se conserva la supresión de ruido casi plano.
- El clamp estricto al mínimo/máximo local anulaba el realce en máximos/mínimos finos. Ahora admite un margen RGB acotado: 3,125 % del rango local a intensidad 25, hasta 12,5 % a 100. No garantiza ausencia perceptual de halos; limita su amplitud y requiere valoración en movimiento.
- Mismas 96 muestras por bloque de 64 píxeles con gestión de color, misma caché y barrera, misma pasada. La ruta passthrough sigue usando muestras directas. No se filtra Steam/QAM.
- No se han cambiado el valor guardado ni la escala del control Decky. Empezar por 25; comparar apagado/25/50 en la misma escena. No hace falta ponerlo a 100 para probarlo.

Fuente completa actual del parche: [0010-gamescope-downsample-sharpener.patch](../../gamescope/versions/3.16.30-gbm/patches/0010-gamescope-downsample-sharpener.patch). La copia redundante de `prototypes` se eliminó al ordenar el proyecto. El 0010 ocupa ahora la última posición de `patches/series`; los nueve primeros siguen representando la base estable 2.5. No es un parche incremental sobre 0.2: para actualizar un árbol con 0.2 aplicado, revertir primero su parche anterior (guardado en `validation/v0.3/v0.2.patch`).

## Rendimiento medido

Microbenchmark Vulkan en RTX 2060, fuente sintética 3840×2160 → 2560×1440, intensidad 25, 160 muestras por shader tras calentamiento, rondas intercaladas. Mide compute, **no FPS ni presentación del juego**.

| Ruta | Mediana 0.2 | Mediana 0.3 | Diferencia | p95 0.2 → 0.3 |
|---|---:|---:|---:|---:|
| SDR con LUT | 2,447488 ms | 2,449216 ms | +0,001728 ms | 3,121408 → 3,124832 ms |
| Passthrough | 0,389728 ms | 0,394592 ms | +0,004864 ms | 0,400224 → 0,403872 ms |

Coste prácticamente igual en estas pruebas, no coste adicional cero garantizado. En passthrough hay una pequeña subida (~1,25 % del shader), no se oculta. Una repetición SDR con orden inverso dio 2,448288 → 2,450240 ms. La primera medición, concurrente con compilación y con relojes no fijados, dio 2,465888 → 2,906144 ms; se conserva en `sdr.log` y no se usa como resultado estable. No se fijaron relojes ni se alteró la sesión del usuario.

Logs: `validation/v0.3/sdr-final.log`, `sdr-repeat.log`, `passthrough-final.log`. El loader emite `Authorization required, but no authorization protocol specified` al explorar plataformas gráficas; el benchmark headless completa correctamente en RTX 2060 (código 0). No hay validación KMS/VRR ni salida HDR real.

## Respuesta y verificaciones

En la región de textura sintética, la diferencia media frente al filtro apagado a intensidad 25 pasa de 0,212 a 0,707 niveles RGB de 8 bits (~3,33×). A 50 y 100 la respuesta también aumenta progresivamente. **No significa 3,33× más calidad o detalle recuperado**, solo mayor efecto numérico a igual control.

- Shader validado mediante `spirv-val --scalar-block-layout` y Gamescope compilado; `--version` correcto.
- Filtro apagado idéntico byte a byte respecto a 0.2; bordes de pantalla y alpha intactos.
- Overshoot dentro del margen definido a 25/50/100, comprobado en luz lineal con tolerancia de cuantización.
- Ejecución en tamaño impar 257×145, rotación, letterbox y overlay completada. No equivale a una valoración visual en juego.
- Parche aplicable a una base limpia, reversible, `git diff --check` correcto. Se comprueba que el único archivo diferente entre parches 0.2 y 0.3 es el shader.
- Instalador simulado en directorio temporal: instalación, hashes, idempotencia y rollback manual/ante fallo. No se ejecuta sudo.
- Huellas de Decky comprobadas antes/después sin cambios.

[Comparación sintética](validation/v0.3/comparacion-sintetica.png), [métricas](validation/v0.3/image-metrics.json), [comprobación de límites](validation/v0.3/check_bounds.py).

## Instalación y recuperación

Instalador actual (sin backups): `validation/v0.3/install-local-0.3.0.sh`.

```bash
cd /ruta/a/Ubuntu-Gaming-Mode
prototypes/downsampling-sharpener/validation/v0.3/install-local-0.3.0.sh --check
sudo prototypes/downsampling-sharpener/validation/v0.3/install-local-0.3.0.sh
```

Payload conservado en `.build/downsampling-sharpener/releases/0.3.0/`, dentro del proyecto y fuera de Git. Sustituye solo `/usr/lib/ubuntu-gaming-mode/gamescope`, comprueba hashes y conserva propietario, permisos y atributos. No toca Decky ni reinicia servicios. No crea copia de seguridad: para volver a UGM 2.5, reinstalar el DEB publicado en Git.

Entrar en modo gaming carga el binario nuevo. Mantener Decky 0.2 sin reinstalarlo. Diagnóstico de solo lectura: `validation/diagnose-session.py`.

Los ensayos de rollback descritos arriba corresponden al instalador anterior; el actual no tiene rollback ni conserva el binario antiguo.

SHA256 Gamescope 0.3: `3260eaa4407623a02faed105dcfc2140be68ede64ac41e7bd979a41177cd1246`.

Pendiente: misma escena a 0/25/50, texturas y vegetación en movimiento, y comparación de frametimes. La preferencia visual y la conservación de fluidez dentro del juego requieren esa prueba; no se prometen a partir del microbenchmark.
