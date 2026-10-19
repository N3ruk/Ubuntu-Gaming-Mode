# Capa VRS incorporada a UGM 2.8

[English](../../ugm-2.8/vrs.md) | **Español** · [Resumen 2.8](README.md)

Fecha de integración: 2026-10-17. Las fuentes están incorporadas al paquete
publicado 2.8.0-2; esta sección conserva la procedencia de la importación.

## Procedencia y alcance

Origen: árbol de trabajo `UGM-vrs-audit`, HEAD
`9aa1262da897de1b478c38e276acf720d1fd32c1` **más sus cambios sin commit**.
El [manifiesto SHA256](vrs-import-manifest.json) identifica exactamente los
archivos trasladados; el commit por sí solo no contiene la versión incorporada.

Se incluyen:

- `prototypes/vrs-common/`: núcleo, política SPIR-V, GPL, diagnóstico y pruebas.
- `prototypes/nvidia-vrs/` y `prototypes/amd-vrs/`: adaptadores, build y fixtures
  originales de regresión; las bibliotecas/manifiestos se generan localmente.
- `src/ugm-vrs-stage`, `src/ugm-vrs-select`, cambios exclusivos de selección VRS
  en `src/ugm-capabilities` y del bloque VRS de `system/gamescope-session/steam`.
- Pruebas de integración y capacidades correspondientes.

No se incorpora el resto del worktree de investigación, ni `.build`, ni capturas,
sondas de timestamps/identidades, shaders de juegos o cachés descartadas.
`src/ugm-vrs-activate` ya era idéntico y no necesitó cambios. El empaquetado
existente invoca los adaptadores; no se han cambiado sus opciones, Gamescope,
Decky, el despertar Bluetooth/USB ni la política de HDR/VRR.

## Mejoras frente a la capa anterior de este proyecto (2.5)

### 1. Núcleo compartido y corrección de ciclo de vida

AMD y NVIDIA usan la misma implementación y política, con adaptadores que
comprueban fabricante/driver. Dispatch por instancia y dispositivo, registros
independientes, handles completos en i386 y seguimiento de resultados válidos
en lotes parcialmente fallidos. Los fallos de reserva de memoria conservan
la ruta original cuando no se puede instrumentar con seguridad.

### 2. Menor coste CPU de la capa

- Tablas hash por dispositivo para shaders/pipelines en vez de recorridos
  lineales; menos contención global y eliminación del seguimiento innecesario
  de command buffers. Se conserva la inicialización de tasa en cada bind.
- Lectura QAM mediante metadatos síncronos `statx`: solo se vuelve a abrir/leer
  el archivo si cambia. No se añade polling ni un TTL; archivos inválidos,
  especiales o errores hacen volver a 1×1. Se conserva compatibilidad ABI
  de las bibliotecas 32/64 bits con el runtime probado.
- Última optimización GPL: elimina **cuatro reservas temporales por llamada/lote
  exclusivamente GPL**, y un bloqueo vacío. No divide ni reordena los lotes;
  conserva allocator, parámetros, `VkResult` y handles de éxito parcial.

El microbenchmark histórico con driver simulado midió, para los casos GPL
individuales, unos 47–48 ns menos en 64 bits y 74–75 ns en 32 bits. Los controles
monolíticos/mixtos tuvieron un pequeño sobrecoste de detección (aprox. 0,55–2,21 %
en aquella serie). **No es una medida de compilación del driver ni demuestra
menos stutter, mejor arranque o más FPS.** Las dos cachés experimentales que
empeoraban el rendimiento se descartaron y no están en estas fuentes.

### 3. Política de shaders más conservadora y compatible

Se protege W=1, caminos mixtos y operaciones sensibles como FragCoord, descarte,
derivadas explícitas, profundidad, helper invocations, escrituras/atómicas y
semánticas no reconocidas. Se conservan límites de análisis, exclusiones de
blending/sample shading y protección del VRS controlado por la aplicación.
No se reescriben shaders ni se aplica una lista de juegos.

A la vez se reconocen formas demostrables de helpers puros, transformaciones
escalarizadas, capacidades declarativas modernas y raíces de almacenamiento
con índices dinámicos. Los identificadores Vulkan de shaders previamente
observados pueden recuperar el análisis desde una tabla acotada por dispositivo;
identificadores desconocidos no autorizan sombreado grueso.

La heurística de posición/W **no es un detector infalible de interfaz**.
Más protección puede reducir cobertura frente a la capa antigua; no se afirma
que todos los juegos se beneficien.

### 4. Compatibilidad GPL y flags2

Soporte conservador de Graphics Pipeline Library: resolución de subconjuntos,
enlaces anidados y estados locales/enlazados; variantes privadas coherentes para
pre-rasterización y fragmento sin alterar las bibliotecas originales. Si falta
información o falla la preparación, se conserva el pipeline original. Se
reconocen flags2 compatibles, incluidos los de descriptor heap; esa ruta exige
soporte real del dispositivo. Las variantes pueden añadir compilación y memoria.

### 5. Carga e integración de sesión

La compilación publica bibliotecas por rename y el staging publica una generación
completa e inmutable por hash. No trunca bibliotecas que puedan estar mapeadas y
no elimina generaciones usadas por procesos vivos. Se respetan `VK_LAYER_PATH`,
`VK_ADD_LAYER_PATH` y otras capas explícitas. Varias pantallas en una única GPU
verificada ya no bloquean VRS por confundirlo con una selección de pantalla
ambigua; las decisiones HDR/VRR no cambian y una GPU ambigua sigue sin habilitarse.

### 6. Diagnóstico y opciones experimentales, desactivados por defecto

- `UGM_VRS_DEBUG=1`: decisiones y motivos de exclusión; los contadores de binds
  **no** son píxeles, draws, tiempo GPU ni porcentaje del fotograma.
- `UGM_VRS_AUDIT_ONLY=1`: conserva el filtro anterior al comparar reglas genéricas.
- `UGM_VRS_CAPTURE_DIR`: captura local acotada, solo al indicar directorio;
  no se habilita ni se incluye ninguna captura en el proyecto transferido.
- `UGM_VRS_EXPERIMENTAL_RATE=1x2|2x1`: sustituye solo una petición QAM 2×2;
  QAM 1×1 sigue desactivando el sombreado grueso.
- `UGM_VRS_EXPERIMENTAL_MSAA=1`: pruebas opt-in de MSAA estático 2/4/8,
  comprobando soporte de cada combinación tasa/muestras; conserva las demás
  protecciones y vuelve a 1×1 en combinaciones no soportadas.

**El QAM público sigue en 1×1/2×2 y el MSAA ampliado no se activa por defecto.**
La investigación previa no justificó habilitarlo globalmente. No se añade una
excepción para el shader caro de Tomb Raider descartado por seguridad.

## Validación y límites

La investigación de origen documentó matriz CPU 32/64 bits y sanitizers, más
72 casos GPU del último cambio GPL (48 normales y 24 con Khronos Validation
cargada) sobre NVIDIA RTX 2060. Son resultados históricos; no deben presentarse
como nuevas pruebas del paquete 2.8. AMD tiene compilación y mocks, **no GPU
física validada**. Calidad temporal y consumo a FPS fijos siguen pendientes.

La última optimización GPL no amplía cobertura VRS ni acredita una nueva mejora
de FPS. La copia al proyecto de build tampoco establece una ganancia de juegos.

### Verificación de esta incorporación

Ejecutado en este proyecto destino el 2026-10-17:

- **66 archivos** de fuentes/scripts/pruebas cotejados con el origen por SHA256 y
  permisos; incluye cambios sin commit. El árbol fuente de auditoría se conserva.
- **77 pruebas Python** del proyecto: correctas.
- **Matriz CPU AMD/NVIDIA 32/64 bits**, fixtures SPIR-V optimizados/sin optimizar,
  AddressSanitizer y UndefinedBehaviorSanitizer: correcta.
- **60 casos GPU NVIDIA RTX 2060**: 12 básicos y 48 de GPL directo/anidado,
  auditoría y VRS de la aplicación, en 32/64 bits: correctos. En esta incorporación
  no se repitió Khronos Validation; sus resultados anteriores son históricos.
- Payload VRS generado usando las mismas opciones de salida/rutas que
  `packaging/build-deb.sh`: cuatro ELF de arquitectura correcta, **idénticos byte
  a byte a los del worktree de auditoría**, manifiestos válidos y staging de ambas
  rutas probado en un directorio aislado dentro de `.build`.
- Sintaxis de scripts y `git diff --check`: correctos. Los 168 archivos previos
  fuera del alcance permitido se mantienen idénticos en contenido y permisos.

[Resultados y hashes](vrs-verification.json). Los logs completos se conservan en
`.build/vrs-import-20261017/` (artefactos locales ignorados por Git). En el momento de esta verificación no se habían construido el `.deb` completo
ni el nuevo Gamescope. Ambos se construyeron después para la release publicada 2.8.0-2.

### Reproducción desde la raíz del proyecto

```sh
python3 -m unittest discover -s tests
bash prototypes/vrs-common/tests/run.sh
# GPU NVIDIA compatible; fixtures anteriores necesarios:
bash prototypes/nvidia-vrs/tests/run-gpu.sh
bash prototypes/vrs-common/tests/run-gpl-gpu.sh
```

Requiere compiladores/libc de 32/64 bits, cabeceras y loader Vulkan,
`glslangValidator`, `spirv-val` y `spirv-as`. Las pruebas GPU son offscreen.
Las herramientas viven en el proyecto y no se incluyen en el payload de ejecución
del `.deb`; este contiene las cuatro bibliotecas y sus manifiestos, inertes hasta
que la ruta seleccionada de sesión las active. La [documentación general de UGM 2.8](README.md) distingue estas pruebas
de VRS de la validación del paquete completo.
