# Ubuntu Gaming Mode para Ubuntu 26.04

[English](README.md) | **Español**

**Ubuntu Gaming Mode (UGM)** ofrece un modo juego estilo SteamOS para **Ubuntu 26.04** basado en una sesión dedicada de **Gamescope DRM/KMS** y Steam Gamepad UI.

Permite cambiar entre el escritorio normal de Ubuntu y una sesión de juego tipo consola sin sustituir Ubuntu ni convertir el sistema en otra distribución.

## Versión actual

**Ubuntu Gaming Mode 1.0.0-3 — GOLD**

Validado en Ubuntu 26.04 con una NVIDIA GeForce RTX 2060 y el runtime de Gamescope incluido en el paquete:

- Gamescope: `3.16.28-3-g0d07f6e`
- sesión Gaming Mode dedicada sobre DRM/KMS
- cambio Desktop ↔ Gaming Mode
- salida 4K
- HDR
- VRR / Adaptive Sync
- Steam Overlay
- soporte de mando
- MangoApp / MangoHud
- escalado FSR
- escalado NIS, desbloqueado mediante el plugin de Decky Loader [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector)

### SHA256 de la versión

```text
959afa06fd87eeeba8ffc1d4a3a6c965f9fa902d5678d43597b2ce7b20dca797  ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

## Instalación

Descarga el `.deb` y `SHA256SUMS` desde la misma GitHub Release, verifica el checksum e instala el paquete:

```bash
sha256sum -c SHA256SUMS
sudo apt install ./ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

El paquete ejecuta automáticamente su diagnóstico final durante la instalación. También puede ejecutarse manualmente en cualquier momento:

```bash
sudo ubuntu-gaming-mode-doctor
```

Una instalación saludable de 1.0.0-3 devuelve:

```text
OK:       54
Warnings: 0
Fallos:   0
```

Consulta la [guía de instalación en español](docs/es/instalacion.md) para más detalles.

## Cambiar entre Desktop y Gaming Mode

Desde Ubuntu Desktop, ejecuta **Volver a Gaming Mode**.

El lanzador prepara la siguiente sesión de GDM, solicita confirmación, cierra limpiamente la sesión de Ubuntu y entra en Steam Gaming Mode.

Al volver desde Gaming Mode, UGM restaura la sesión de escritorio de Ubuntu configurada.

## Desinstalación y rollback

Para eliminar UGM y restaurar el baseline capturado antes de la primera instalación:

```bash
sudo apt purge ubuntu-gaming-mode
```

UGM conserva un snapshot inmutable del estado original, de forma que una actualización no sustituya la referencia utilizada para el rollback.

Consulta [Desinstalación y rollback](docs/es/desinstalacion-y-rollback.md).

## Rendimiento de Gamescope: DRM frente a modo nested

UGM existe específicamente para evitar que Gamescope se ejecute nested dentro de GNOME/Mutter durante Gaming Mode.

Mediciones en Dragon Ball Z: Kakarot, con resolución interna 1920×1080 y salida 2560×1440:

| Escenario | Linear | FSR / NIS |
|---|---:|---:|
| Gamescope nested, SDL | ~96 FPS | ~88–91 FPS |
| Gamescope nested, Wayland | ~101–102 FPS | ~88 FPS |
| Gamescope DRM / Gaming Mode | ~120 FPS | ~114–117 FPS |

En esta prueba, la sesión DRM dedicada ofreció aproximadamente **+18–25 %** de rendimiento en linear y **+25–33 %** con FSR/NIS frente a las rutas nested probadas.

Metodología, resultados e interpretación completos: [Rendimiento FSR/NIS: Gamescope nested frente a DRM](docs/es/rendimiento-fsr-nis-gamescope-nested-vs-drm.md).

## Arquitectura

Ruta simplificada de Gaming Mode:

```text
Juego
  -> Gamescope
  -> FSR/NIS cuando está activado
  -> DRM/KMS
  -> Pantalla
```

Esto elimina GNOME/Mutter de la ruta final de presentación del juego.

Para NIS, UGM utiliza [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector), un plugin de Decky Loader que desbloquea y permite seleccionar el filtro dentro de Gaming Mode.

Consulta la [arquitectura de Ubuntu Gaming Mode](docs/es/arquitectura.md).

## Comportamiento importante

UGM gestiona:

- la sesión Steam Gaming Mode de GDM/Wayland;
- la sesión seleccionada en AccountsService;
- los valores de autologin necesarios para cambiar entre Desktop y Gaming Mode;
- una regla sudoers mínima utilizada únicamente por el servicio de rearme del autologin;
- su propio runtime de Gamescope bajo `/usr/lib/ubuntu-gaming-mode/`.

La instalación y las actualizaciones **no reinician GDM ni cierran la sesión actual**.

## Código fuente y empaquetado

El código y los archivos del paquete están publicados en este repositorio:

- `src/` — scripts de runtime y helpers de sesión de UGM
- `system/` — unidades systemd, entradas de escritorio/sesión y configuración de Gamescope
- `packaging/DEBIAN/` — control y scripts de mantenimiento de Debian
- `packaging/build-deb.sh` — script de ensamblado del paquete
- `gamescope/` — información sobre el runtime custom de Gamescope validado

Consulta [Estructura del código fuente y reproducibilidad](SOURCE_es.md).

El binario custom de Gamescope y el icono de la aplicación no se almacenan dentro del historial Git; son entradas del proceso de build y están incluidos en el artefacto de release validado.

El Gamescope incluido en 1.0.0-3 **no es una build upstream sin modificar**. Durante el desarrollo de UGM se adaptó para funcionar correctamente a 4K y para interoperar con [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) mediante un pequeño conector de integración. El árbol de código fuente modificado se eliminó posteriormente, por lo que ya no es posible reconstruir de forma fiable el conjunto exacto de cambios. Si en una versión futura de UGM es necesario actualizar Gamescope, esa integración se volverá a implementar sobre la nueva base y se documentará entonces.

## Documentación en español

- [Instalación](docs/es/instalacion.md)
- [Desinstalación y rollback](docs/es/desinstalacion-y-rollback.md)
- [Arquitectura](docs/es/arquitectura.md)
- [Rendimiento FSR/NIS: Gamescope nested frente a DRM](docs/es/rendimiento-fsr-nis-gamescope-nested-vs-drm.md)
- [Estructura del código fuente](SOURCE_es.md)
- [Runtime custom de Gamescope](gamescope/README_es.md)
- [Historial de cambios](CHANGELOG_es.md)

## Estado

1.0.0-3 es la primera versión candidata promovida a **GOLD** después de validar instalación limpia, actualización, purge/rollback y el ciclo físico Desktop ↔ Gaming Mode.

## Licencia

La licencia todavía no está publicada en este repositorio. Los componentes de terceros conservan sus respectivas licencias.
