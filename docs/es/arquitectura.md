# Arquitectura de Ubuntu Gaming Mode: Gamescope DRM/KMS en Ubuntu 26.04

[English](../architecture.md) | **Español**

Ubuntu Gaming Mode proporciona una sesión dedicada de Steam/Gamescope junto al escritorio normal de Ubuntu.

## Ruta de juego en el escritorio

Una ruta conceptual de juego bajo el escritorio de Ubuntu es:

```text
Juego
  -> Gamescope (cuando se ejecuta nested)
  -> superficie Wayland/SDL
  -> GNOME Mutter
  -> DRM/KMS
  -> Pantalla
```

Cuando Gamescope funciona nested, Mutter permanece dentro de la ruta final de presentación.

## Ruta Gaming Mode de UGM

UGM inicia Gamescope como compositor de la sesión de juego:

```text
Juego
  -> Gamescope
  -> FSR/NIS cuando está activado
  -> DRM/KMS
  -> Pantalla
```

De esta forma GNOME/Mutter queda fuera de la ruta final de presentación del juego.

Para NIS, el flujo de UGM utiliza el plugin de Decky Loader [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) para desbloquear y seleccionar el filtro dentro de Gaming Mode.

## Runtime canónico de Gamescope

UGM utiliza su propio binario validado de Gamescope:

```text
/usr/lib/ubuntu-gaming-mode/gamescope
```

Versión validada:

```text
gamescope 3.16.28-3-g0d07f6e
```

SHA256 validado:

```text
e43f0737287b2812d0c34a43c638131b3656058e1666d55228ad8acfe59d616c
```

Este binario de Gamescope es una build específica del proyecto. Durante el desarrollo de UGM se modificó para la ruta 4K validada y para interoperar con [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) mediante un pequeño conector. El árbol de código fuente modificado original ya no existe, por lo que el diff exacto de Gamescope no está documentado para 1.0.0-3.

El wrapper/runtime de la sesión también se instala bajo `/usr/lib/ubuntu-gaming-mode/`.

SHA256 validado del runtime:

```text
3671cbc698b05e6ae3e68492848af3fec33aa0fe64c2f94a5e3e3395b498e636
```

## DRM scanout

La sesión Steam validada conserva:

```bash
export gamescope_drm_gbm_scanout=1
```

El conector DRM se detecta automáticamente en lugar de fijarse a un conector concreto como `HDMI-A-1`.

## Sesión de Steam

UGM ejecuta Steam con una configuración orientada a consola, utilizando Gamepad UI y flags orientados a SteamOS.

El paquete proporciona:

```text
/usr/share/wayland-sessions/steam-gaming-mode.desktop
```

## Cambio Desktop ↔ Gaming Mode

UGM utiliza AccountsService para seleccionar la sesión del siguiente login de GDM.

El lanzador Desktop → Gaming:

1. comprueba que se está ejecutando desde la sesión de escritorio configurada;
2. solicita confirmación al usuario;
3. selecciona `steam-gaming-mode`;
4. rearma el estado de autologin necesario mediante un servicio de sistema de alcance limitado;
5. cierra GNOME limpiamente.

La ruta Gaming → Desktop selecciona la sesión de escritorio de Ubuntu configurada antes de volver.

## Límite de privilegios

El flujo normal de cambio de sesión se ejecuta como usuario de escritorio.

Solo la operación específica necesaria para rearmar el autologin se expone mediante una regla sudoers mínima.

## Diagnóstico

`ubuntu-gaming-mode-doctor` valida el runtime del paquete, hashes de Gamescope, configuración de sesión, unidades systemd, sudoers, GDM, AccountsService, estado de rollback, DRM, GPU y disponibilidad de Steam.

Desde 1.0.0-3 también comprueba que `session.conf` sea realmente legible por el usuario UGM configurado, evitando la regresión de permisos detectada durante la aceptación física de 1.0.0-2.
