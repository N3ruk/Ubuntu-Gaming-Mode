# NVIDIA VRS: política inspirada en RADV (2026-10-09)

Estado en `2.5.0-5`: esta fuente se compila para 32/64 bits durante el build
del paquete y se entrega en `vrs-bundled`. La sesión copia la ruta elegida desde
ese payload legible, no desde `/var/lib/ubuntu-gaming-mode` (privado). Las notas
manuales de instalación al final son históricas; la integración vigente está
en `system/gamescope-session/steam` y `src/ugm-vrs-stage`.

La capa conserva el interruptor global existente **1×1/2×2**. No usa perfiles ni
listas de juegos. El selector 2×1 de un experimento anterior no se reactiva.

## Selección implementada

- Analiza SPIR-V sin modificar los shaders. Reconoce `BuiltIn FragCoord`,
  incluidos miembros de estructuras, y preserva esos fragment shaders. También
  preserva entradas por muestra, descarte, interpolación flat y VRS nativo.
- Sigue `BuiltIn Position` directo o dentro de `gl_PerVertex`: constantes,
  composiciones, extracciones, inserciones, shuffles, copias, selects, phis y
  cargas locales sencillas. Si las escrituras reconocidas de W son 1, conserva
  la tasa original (normalmente 1×1). W distinto de 1 o dependiente de datos puede
  optar a 2×2, siguiendo la intención de la heurística de RADV.
- Conserva las exclusiones anteriores de blending y sample shading. MSAA,
  alpha-to-coverage, etapas de geometría/teselación/mesh, bibliotecas incompletas,
  módulos con varias entradas, grupos de decoraciones, llamadas y flujo no
  resuelto no se fuerzan. El análisis tiene límites de tamaño, IDs, profundidad
  y trabajo; no es un validador SPIR-V completo.
- La tasa sigue leyéndose al grabar `vkCmdBindPipeline`; no cambia comandos ya
  grabados. La aplicación mantiene su memoria de creación intacta.
- También se reenvían los binds de dispositivos excluidos/inactivos. Se reserva
  su seguimiento antes de crear comandos; se conservan pipelines válidos en
  lotes parcialmente fallidos sin dejar un estado dinámico sin inicializar.

## Diferencias frente a AMD

No es el compilador NIR ni una implementación de VRS por vértice. La decisión
se aplica al pipeline completo, sin reescribir shaders ni inyectar salidas de
PrimitiveShadingRate. No realiza el plegado de constantes y análisis completo
de Mesa: ciertas expresiones equivalentes a W=1 podrían no reconocerse; otras
rutas se excluyen conservadoramente y perderán oportunidades de ahorro. La
mera declaración de FragCoord basta para proteger el módulo, aunque esa entrada
no llegue a usarse. Tampoco es una máscara espacial ni un detector infalible de
interfaz. No garantiza imágenes o rendimiento idénticos a AMD en todos los juegos.

Referencias revisadas:

- [RADV: gather_intrinsic_store_output_info y disallow_force_vrs_per_vertex](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/amd/vulkan/radv_shader_info.c)
- [Mesa: RADV_FORCE_VRS](https://docs.mesa3d.org/envvars.html#envvar-RADV_FORCE_VRS)

## Pruebas reproducibles

```sh
bash prototypes/nvidia-vrs/build.sh
bash prototypes/nvidia-vrs/tests/run.sh
bash prototypes/nvidia-vrs/tests/run-gpu.sh
```

Requieren compiladores de 32/64 bits, cabeceras Vulkan, loader Vulkan para ambas
arquitecturas, glslangValidator y spirv-val. La prueba GPU requiere NVIDIA con
VRS; renderiza fuera de pantalla y no abre juegos ni cambia la sesión.

Verificado en RTX 2060: shaders SPIR-V válidos optimizados y sin optimizar;
selección y passthrough 32/64 bits; ASan/UBSan y mutaciones del analizador;
renderizado diferencial con cambio de archivo 1×1/2×2 dentro del proceso.

| Escena de prueba (64×64) | Píxeles distintos con 2×2 |
| --- | ---: |
| Geometría elegible (W=2) | 2048 / 4096 |
| Interfaz/pantalla completa con W=1 | 0 / 4096 |
| Fragment shader que usa FragCoord | 0 / 4096 |
| Geometría gruesa seguida de interfaz en el mismo command buffer | 0 / 4096 |
| Dispositivo con estructura VRS proporcionada por la aplicación (passthrough) | 0 / 4096 en las cuatro escenas |

La GPU de laboratorio comprueba la aplicación real de la tasa, no solo trazas.
No se instaló ni ejecutó `VK_LAYER_KHRONOS_validation` (no está disponible).
**Pendiente:** resultado visual/rendimiento en Kakarot y demás juegos reales.

Las bibliotecas instaladas deben reemplazarse mediante rename atómico, nunca
sobrescribirse mientras un proceso pueda tenerlas mapeadas. Procesos existentes
conservan su versión hasta cerrarse y abrirse de nuevo. Antes de instalar,
conservar una copia de las bibliotecas anteriores; no se necesitan cambios en
Decky, QAM, lanzadores ni configuración global para esta actualización.
