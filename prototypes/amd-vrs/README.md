# VRS AMD experimental (RADV)

Capa Vulkan de UGM para GPU AMD integradas o dedicadas con Mesa RADV,
`VK_KHR_fragment_shading_rate`, `pipelineFragmentShadingRate` y tasa 2×2 con
una muestra. No depende de que la GPU sea una APU GFX10.3. El selector de
instalación exige una única GPU Vulkan hardware coincidente con la salida DRM
y un inventario positivo; la capa repite la comprobación de driver y capacidad
al crear cada dispositivo. No es compatible con AMDVLK ni con el driver Vulkan
propietario de AMD.

La política de shaders y pipelines se copió de la capa NVIDIA existente para
mantener un comportamiento conservador; los cambios en esta ruta no afectan
al binario NVIDIA. El QAM de Steam escribe `RADV_FORCE_VRS_CONFIG_FILE` y UGM
entrega ese mismo archivo mediante `UGM_VRS_CONFIG_FILE` a esta capa. La tasa
se lee al grabar `vkCmdBindPipeline`: los command buffers ya grabados no cambian
hasta que el juego los vuelva a grabar. En APU GFX10.3, Mesa puede consumir
además su propio archivo dinámico; no se garantiza equivalencia visual exacta.

La opción se ofrece en debconf como **EXPERIMENTAL FEATURE**, desactivada por
defecto. Cambiar GPU o driver requiere reinstalar o ejecutar
`dpkg-reconfigure ubuntu-gaming-mode` para volver a evaluar la ruta. No se ha
validado todavía el efecto visual ni el rendimiento en una GPU AMD real.

Pruebas locales:

```sh
bash prototypes/amd-vrs/build.sh
python3 -m unittest discover -s tests -p test_amd_vrs_integration.py
```

En un equipo AMD/RADV, el test diferencial real se ejecuta con:

```sh
bash prototypes/nvidia-vrs/tests/run.sh
bash prototypes/amd-vrs/tests/run-gpu.sh
```

Debe producir diferencias para geometría elegible al pasar 1×1→2×2 y conservar
interfaz/posprocesado excluidos. Esta prueba todavía **no se ha ejecutado en
AMD** en el entorno de desarrollo actual.
