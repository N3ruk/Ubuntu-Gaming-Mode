# Capa VRS AMD experimental — UGM 2.8

Adaptador para Mesa RADV con `VK_KHR_fragment_shading_rate`,
`pipelineFragmentShadingRate` y tasa compatible. No admite AMDVLK ni el driver
propietario AMD. Comparte [núcleo y política](../vrs-common/README.md) con NVIDIA;
no duplicar su implementación. [Cambios y límites](../../docs/es/ugm-2.8/vrs.md).

Ruta experimental con consentimiento de instalación, desactivada por defecto.
Se conserva la selección conservadora de GPU y la ruta nativa RADV existente;
no combinar la capa con VRS nativo forzado. El QAM mantiene 1×1/2×2 y la tasa se
aplica al grabar binds, no modifica comandos ya grabados. Las bibliotecas 32/64
se generan desde fuente y la sesión publica una generación inmutable.

```sh
# Desde la raíz del repositorio:
bash prototypes/amd-vrs/build.sh
python3 -m unittest discover -s tests -p test_amd_vrs_integration.py
bash prototypes/vrs-common/tests/run.sh
# Solo en una GPU AMD/RADV compatible:
bash prototypes/amd-vrs/tests/run-gpu.sh
UGM_TEST_ROUTE=amd bash prototypes/vrs-common/tests/run-gpl-gpu.sh
```

**AMD: compilación y mocks verificados; calidad/rendimiento en GPU física no
validados.** Los resultados de NVIDIA no certifican hardware AMD. Cambiar GPU o
driver requiere reevaluar la ruta mediante instalación/reconfiguración de UGM.
