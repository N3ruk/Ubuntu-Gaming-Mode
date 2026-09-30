# Manifiesto de Ubuntu Gaming Mode 2.0.0-1

## Componentes incluidos en el paquete

- Runtime Gamescope 3.16.30 GBM validado, instalado únicamente bajo
  `/usr/lib/ubuntu-gaming-mode/gamescope`.
- Lanzadores y runtime de `gamescope-session-plus`.
- Selector Desktop/Gaming, retorno a Gaming Mode y rearme de autologin.
- Doctor y asistente de configuración de UGM.
- Marcador de sesión Gaming administrada.
- Puente opcional del botón físico y helper `systemd --user` para el flujo
  nativo `steam://shortpowerpress`.
- Helper reversible opcional para publicar el runtime validado de UGM como
  `/usr/local/bin/gamescope`, sin desinstalar el paquete Gamescope existente.
- Unidades systemd de sistema y usuario.
- Definición de sesión Wayland, acceso de aplicación e icono.
- Configuración Steam Gaming Mode para DRM, GBM scan-out, HDR, VRR,
  escalado y MangoApp.
- Scripts Debian para instalación, actualización, desinstalación, purge y
  restauración del estado anterior.

## Código fuente conservado fuera del binario DEB

El repositorio conserva la base exacta y la serie reproducible de Gamescope:

- base upstream 3.16.30: `ad2763da1c48860f649abfe842a087188dcb6e20`;
- patches 0001–0008: port GBM derivado de upstream;
- patch 0009: fallback de seguridad UGM;
- receta de build, auditoría, validación y hashes.

Estos materiales documentales y patches no se instalan en el sistema final;
sirven para reproducir y auditar el runtime incluido.
