# Capa VRS NVIDIA — UGM 2.8

Adaptador para NVIDIA propietario con soporte Vulkan de fragment shading rate.
El código está en [el núcleo compartido](../vrs-common/README.md); no mantener
una segunda copia del analizador. [Cambios y validación](../../docs/es/ugm-2.8/vrs.md).

El build del paquete genera bibliotecas y manifiestos 32/64 en `vrs-bundled`.
La sesión seleccionada prepara una generación inmutable mediante `ugm-vrs-stage`;
no se registra una capa global ni se modifica el QAM 1×1/2×2. VRS de la aplicación,
estados no demostrados y fallos conservan la ruta original.

```sh
# Desde la raíz del repositorio:
bash prototypes/nvidia-vrs/build.sh
bash prototypes/vrs-common/tests/run.sh
bash prototypes/nvidia-vrs/tests/run-gpu.sh
bash prototypes/vrs-common/tests/run-gpl-gpu.sh
```

Las pruebas renderizan fuera de pantalla. El código se ha probado en RTX 2060,
pero no garantiza calidad temporal o ganancia de FPS universal. La heurística
W/posición no es un detector infalible de interfaz. Las tasas experimentales y
el MSAA ampliado siguen siendo opt-in. Consultar la guía para resultados,
protecciones, pruebas con Validation y límites de interpretación.
