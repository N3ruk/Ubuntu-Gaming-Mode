# Prototipos y experimentos

[English](README.md) | **Español** · [Índice de documentación](../docs/es/README.md)

Este directorio contiene **fuentes y herramientas de prueba**, no otro método
de instalación. Algunos componentes están integrados en el paquete 2.8 y otros
archivos son registros históricos. Para instalar usa el `.deb` publicado según
la [guía](../docs/es/instalacion.md), no los scripts de prototipos.

| Componente | Papel en 2.8 | Inicio |
| --- | --- | --- |
| `vrs-common/` | Política VRS compartida y conservadora, diagnóstico y pruebas | [README](vrs-common/README.md) |
| `amd-vrs/`, `nvidia-vrs/` | Fuentes de adaptadores compiladas en capas opcionales 32/64 bits | [AMD](amd-vrs/README.md) · [NVIDIA](nvidia-vrs/README.md) |
| `downsampling-sharpener/` | Fuentes de parche/plugin y validaciones históricas | [README](downsampling-sharpener/README.md) |
| `bluetooth-wake-*.py` | Sondas de desarrollo; no son la app instalada | [Política USB 2.8](../docs/es/ugm-2.8/despertar-usb.md) |
| `ugm-vrs-run` | Lanzador local para experimentos VRS | [VRS 2.8](../docs/es/ugm-2.8/vrs.md) |

La construcción del paquete se define en `packaging/build-deb.sh`. Los
resultados de un prototipo no validan por sí solos el paquete entero ni todo
el hardware.
