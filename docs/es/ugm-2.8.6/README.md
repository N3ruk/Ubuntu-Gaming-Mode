# Ubuntu Gaming Mode 2.8.6

[English](../../ugm-2.8.6/README.md) | **Español** · [Documentación](../README.md)

Paquete **2.8.6-1**, Ubuntu **26.04 amd64**. Cambios respecto a 2.8.5:

- Añadir compatibilidad con el cliente Steam **1794536281** y su nueva API nativa de actualización, conservando el soporte de **1792618285**.
- Corregir las órdenes de Video Output en Ubuntu mediante credenciales nativas de subproceso Python, sin depender de una ruta fija de `runuser`.
- Omitir mandos Bluetooth guardados temporalmente ausentes para evitar fallos repetidos del servicio, conservando sus preferencias de despertar.
- Cambiar el icono del lanzador **Volver a Gaming Mode** por el logotipo UGM del pingüino gamer cyberpunk.

El hook de YouTube, Gamescope, VRS y los demás paneles de UGM Options no cambian.
Consulta la [instalación](../instalacion.md) y la [guía de funciones existentes](../ugm-2.8.5/README.md) para su uso.
