# Núcleo VRS compartido — UGM 2.8

Los adaptadores `../nvidia-vrs/layer.c` y `../amd-vrs/layer.c` fijan fabricante y
driver. Comparten núcleo, política y pruebas en 32/64 bits. QAM sigue en 1×1/2×2.

Ver [cambios, límites y validación](../../docs/es/ugm-2.8/vrs.md) y
[procedencia exacta de las fuentes](../../docs/es/ugm-2.8/vrs-import-manifest.json).
La documentación del proyecto destino sustituye las referencias al worktree de
investigación: no se necesita aquel directorio para compilar o ejecutar pruebas.

| Archivo | Función |
|---|---|
| `layer.c` | Dispatch y seguimiento por dispositivo, selección y creación de pipelines. |
| `compat_policy.h`, `gpl_policy.h` | Estados compatibles, subconjuntos GPL y variantes privadas con fallback. |
| `spirv_policy.h` | Análisis conservador, acotado y de solo lectura. |
| `table.h`, `rate.h` | Índices por dispositivo y consulta QAM síncrona por metadatos `statx`. |
| `shader_identifier.h` | Tabla acotada por dispositivo de identificadores Vulkan ya observados. |
| `diagnostics.h`, `shader_capture.h` | Diagnóstico y captura local opt-in; no activados globalmente. |
| `build.sh` | Build 32/64 y publicación por rename, sin truncar bibliotecas. |
| `launch-diagnostic.sh` | Lanzamiento diagnóstico explícito y aislado; no instala. |
| `tests/` | Regresiones CPU, GPU, sanitizers, generador de fixtures y microbenchmarks. |

```sh
# Desde la raíz del repositorio:
bash prototypes/vrs-common/tests/run.sh
bash prototypes/nvidia-vrs/tests/run-gpu.sh
bash prototypes/vrs-common/tests/run-gpl-gpu.sh
# Solo en hardware compatible:
# UGM_TEST_ROUTE=amd bash prototypes/vrs-common/tests/run-gpl-gpu.sh
# UGM_TEST_HEAP=1 bash prototypes/vrs-common/tests/run-gpl-gpu.sh
```

La matriz CPU compila ambos adaptadores. Las pruebas GPU requieren hardware real
compatible; AMD no está validado físicamente en el equipo de referencia.
Opciones experimentales MSAA/1×2/2×1 y auditoría: ver la guía enlazada; no son
valores predeterminados. Los contadores de binds no miden coste GPU.
