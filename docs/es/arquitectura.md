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

El paquete publicado **2.8.0-2** utiliza la base 3.16.30 GBM y añade el
**parche 0010** de nitidez selectiva al reducir resolución. El SHA256 esperado
del binario Gamescope, fijado en la receta del paquete, es:

```text
4251c390803678e1d3ce626ae4c36b51c00763338efa358ef1d2d5f5c8ef7c03
```

El runtime histórico de **2.5** usaba los parches 0001–0009 y SHA256
`5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.
Su versión incrustada era `gamescope 3.16.30-8-gb211c9d`.
Ese hash **no** identifica el binario 2.8. Consulta el
[registro de fuentes de Gamescope](../../gamescope/README_es.md) y
[el detalle de downsampling](ugm-2.8/downsampling.md). Sharp Filter Selector
usa capacidades expuestas por Gamescope/Steam; la serie GBM no contiene un
conector oculto de selección de filtro.

El wrapper/runtime de sesión también se instala en `/usr/lib/ubuntu-gaming-mode/`.
Su contenido cambió entre versiones; compara los hashes con el paquete
publicado correspondiente, no con el manifiesto de una versión anterior.

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

También comprueba que `session.conf` sea legible por el usuario UGM configurado,
los hashes del runtime empaquetado, la integración opcional del botón físico y
el enlace global opcional de Gamescope.
