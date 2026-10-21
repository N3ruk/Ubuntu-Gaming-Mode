# Instalar Ubuntu Gaming Mode

[English](../installation.md) | **Español** · [Índice de documentación](README.md)

## Requisitos

El paquete publicado `2.8.3-1` está destinado a **Ubuntu 26.04 amd64**.
Instálalo desde una terminal interactiva del escritorio con APT, que resuelve
las dependencias. Las funciones de hardware (HDR, VRR, VRS y despertar
Bluetooth) dependen de la GPU, el controlador, el monitor y el firmware;
consulta [problemas conocidos](problemas-conocidos.md).

## Descargar y verificar

Descarga `ubuntu-gaming-mode_2.8.3-1_amd64.deb` y `SHA256SUMS` de la
[misma release de GitHub](https://github.com/N3ruk/Ubuntu-Gaming-Mode/releases/tag/v2.8.3-1).
Desde el directorio de descarga:

```bash
sha256sum -c SHA256SUMS
sudo apt install ./ubuntu-gaming-mode_2.8.3-1_amd64.deb
```

El comando de checksum debe mostrar `OK`. Deja abierta la terminal para las
preguntas Debconf. El instalador guarda el estado original para rollback,
configura la sesión y ejecuta Doctor; no reinicia GDM ni cierra el escritorio
actual. Una actualización conserva la línea base original.

## Opciones de instalación

Una pregunta opcional del instalador permite instalar y activar UGM-ICONS
en el escritorio del usuario configurado. Está desactivada por defecto. El tema viene en el paquete,
se instala en `~/.local/share/icons/UGM-ICONS` y se selecciona en GNOME. Una
carpeta local previa no se sobrescribe. Con «No» o al quitar UGM se restaura la
selección anterior si UGM-ICONS sigue activo; los archivos se conservan para
proteger posibles modificaciones. Se puede repetir con
`sudo dpkg-reconfigure ubuntu-gaming-mode`.

Debconf ofrece opciones independientes, desactivadas por defecto, para Decky
Loader y sus dos plugins; respeta una instalación previa de Decky. Instalar
Decky requiere red y un fallo en ese paso opcional no cancela UGM. Consulta
[la integración Decky](ugm-2.8/decky.md).

El despertar por mando Bluetooth y su app son opcionales. En **instalaciones
nuevas de 2.8** que activan el despertar, UGM aplica la
[política USB](ugm-2.8/despertar-usb.md). Una actualización desde 2.5 conserva
la política anterior. Hay que probar una suspensión/reanudación real para
comprobar la compatibilidad del equipo.

La ruta VRS APU RADV nativa puede seleccionarse automáticamente en hardware
admitido. Las capas AMD/NVIDIA propias son experimentales y opt-in. No se
garantiza que el botón VRS aparezca ni que todos los juegos ganen rendimiento.
Consulta [VRS](ugm-2.8/vrs.md). El puente para el botón físico y la
publicación del comando global `gamescope` también son opcionales; esta última
no elimina el paquete Gamescope de Ubuntu.

Para revisar las opciones elegidas:

```bash
sudo dpkg-reconfigure ubuntu-gaming-mode
```

## Primer uso y diagnóstico

Ejecuta `sudo ubuntu-gaming-mode-doctor` para revisar el runtime instalado y la
integración de sesión. Desde Ubuntu Desktop, abre **Volver a Gaming Mode** para
preparar el siguiente login GDM; UGM pide confirmación antes de cerrar la
sesión. Al salir de Gaming Mode vuelve a la sesión de escritorio configurada.

Para eliminar UGM y restaurar el estado original capturado:

```bash
sudo apt purge ubuntu-gaming-mode
```

Consulta [desinstalación y rollback](desinstalacion-y-rollback.md). Para
versiones anteriores, usa los [registros históricos](README.md#versiones-anteriores);
no utilices sus nombres de paquete ni hashes al instalar 2.8.
