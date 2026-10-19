# UGM 2.8 — estado del proyecto y novedades

[English](../../ugm-2.8/README.md) | **Español** · [Índice](../README.md)

Actualizado: 2026-10-19. **Paquete actual: 2.8.2-1.**
Este índice documenta las novedades frente a 2.5.0-5 y enlaza sus evidencias.

## Cambios frente a 2.5

| Área | Cambio | Documentación |
|---|---|---|
| Gamescope | Parche 0010: downsampling con nitidez selectiva, filtro 0.4; control opcional mediante plugin 0.2.0. | [Downsampling](downsampling.md) |
| Instalador / Decky | Preguntas independientes, desactivadas por defecto, para Decky Loader, Sharp Filter Selector y UGM Downsampling Sharpener; respeta plugins existentes. | [Decky](decky.md) |
| Despertar Bluetooth / USB | Denegación USB por defecto en instalaciones nuevas que activen despertar Bluetooth; autorización individual de mandos y conservación de la política al actualizar desde 2.5. | [Despertar USB](despertar-usb.md) |
| VRS | Núcleo compartido AMD/NVIDIA 32/64 bits, correcciones de seguridad/compatibilidad, menor coste CPU, soporte GPL y staging inmutable. | [Cambios VRS](vrs.md) |
| Iconos de escritorio (2.8.2-1) | Tema UGM-ICONS opcional, instalado y seleccionado para el usuario configurado tras una pregunta desactivada por defecto. | [Instalación](../instalacion.md) |

Las pruebas específicas de cada componente no equivalen a una validación
universal del paquete completo.

## Incorporación VRS del 17 de octubre

Se trasladaron exclusivamente fuentes, scripts y pruebas VRS desde el árbol de
trabajo `UGM-vrs-audit`, incluidos sus cambios sin commit. No se trasladaron
capturas de juegos, shaders extraídos, sondas de perfil GPU, experimentos
rechazados ni binarios precompilados. El empaquetado compila las cuatro
bibliotecas desde este proyecto. [Alcance y huellas](vrs-import-manifest.json).

Se conservaron Gamescope, parche 0010, integración Decky, política de despertar,
ajustes y metadatos de empaquetado al importar VRS.

## Estado de validación y publicación

- 2.8.0-1 fue el primer paquete local. Su instalación limpia descubrió que
  Decky heredaba el `umask 077` del instalador UGM, dejando `PluginLoader`
  inaccesible para los plugins.
- 2.8.0-2 ejecuta el instalador oficial de Decky con `umask 022`, corrige
  `PluginLoader` a `0755`, fija los permisos del payload y recarga el servicio
  si ya está activo. Pasó 82 pruebas automatizadas antes de la instalación.
- No se ha validado el paquete en todo el hardware ni cada ruta VRS/USB con
  suspensión real. La comprobación física de despertar depende del firmware
  y del equipo.
- Para 2.8.2-1 pasaron 84 pruebas automatizadas y una prueba aislada de
  activación y restauración del tema.
- El `.deb` y `SHA256SUMS` actuales están en la [release publicada](https://github.com/N3ruk/Ubuntu-Gaming-Mode/releases/tag/v2.8.2-1).
  `dist/` es un directorio local de build ignorado por Git.

[Historial completo](../../../CHANGELOG_es.md) ·
[Instalación](../instalacion.md) · [Fuentes](../../../SOURCE_es.md).
