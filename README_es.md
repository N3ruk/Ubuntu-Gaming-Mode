# Ubuntu Gaming Mode para Ubuntu 26.04

[English](README.md) | **Español**

**Ubuntu Gaming Mode (UGM)** añade a Ubuntu Desktop una sesión de juego tipo
consola basada en Gamescope DRM/KMS y Steam Gamepad UI. Permite cambiar entre
el escritorio normal y Gaming Mode sin sustituir Ubuntu por otra distribución.

## Versión actual

**Ubuntu Gaming Mode 2.0.0-1**

- Ubuntu 26.04 amd64.
- Gamescope `3.16.30-8-gb211c9d` con port GBM reproducible.
- Salida 4K, HDR y VRR/Adaptive Sync validados en NVIDIA RTX 2060.
- Steam Overlay, mando y MangoApp/MangoHud.
- Escalado FSR, NIS y SGSR según las capacidades de Gamescope/Steam.
- Cambio Desktop ↔ Gaming Mode mediante GDM y AccountsService.
- Suspensión estilo consola mediante el botón físico, opcional.
- Publicación del Gamescope de UGM como comando global, opcional y reversible.
- Snapshot original, rollback y Doctor integrado.

El `.deb` y su `SHA256SUMS` se publican juntos en la release de GitHub.

## Novedades de 2.0.0-1

### Gamescope 3.16.30 reproducible

UGM incluye un runtime dedicado en:

```text
/usr/lib/ubuntu-gaming-mode/gamescope
```

Su código se reconstruye desde el commit upstream
`ad2763da1c48860f649abfe842a087188dcb6e20` y la serie versionada de nueve
patches de [`gamescope/versions/3.16.30-gbm/`](gamescope/versions/3.16.30-gbm/README_es.md).
Los patches producen el árbol final
`5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.

Los ocho primeros trasladan la ruta de buffers GBM aptos para scan-out. El
noveno añade un fallback seguro cuando falla la importación del semáforo de
reescalado preventivo. En el sistema NVIDIA de referencia esto conserva una
sesión 4K estable con HDR y VRR.

### Botón físico estilo consola

El instalador puede habilitar, de forma **opcional y desactivada por defecto**,
una pulsación corta del botón físico que entrega a Steam su flujo nativo
`steam://shortpowerpress`. Solo se captura en la sesión Gaming Mode gestionada;
el escritorio conserva su comportamiento normal.

### Comando Gamescope global

Otra opción, también desactivada por defecto, permite crear:

```text
/usr/local/bin/gamescope -> /usr/lib/ubuntu-gaming-mode/gamescope
```

No desinstala el Gamescope de Ubuntu. UGM conserva el estado anterior y lo
restaura al desactivar la opción o purgar el paquete.

Ambas opciones pueden cambiarse posteriormente:

```bash
sudo dpkg-reconfigure ubuntu-gaming-mode
```

## Instalación

Descarga el `.deb` y `SHA256SUMS` desde la release, y ejecuta:

```bash
sha256sum -c SHA256SUMS
sudo apt install ./ubuntu-gaming-mode_2.0.0-1_amd64.deb
```

El instalador crea el snapshot original, configura la sesión y ejecuta Doctor.
No reinicia GDM ni cierra la sesión actual.

Diagnóstico manual:

```bash
sudo ubuntu-gaming-mode-doctor
```

Consulta la [guía de instalación](docs/es/instalacion.md).

## Cambiar de modo

Desde Ubuntu Desktop abre **Volver a Gaming Mode**. UGM prepara la siguiente
sesión GDM, solicita confirmación y cierra el escritorio de forma ordenada.
Al salir de Gaming Mode restaura la sesión Ubuntu configurada.

## Desinstalación y rollback

```bash
sudo apt purge ubuntu-gaming-mode
```

UGM restaura el baseline capturado antes de la primera instalación. Consulta
[desinstalación y rollback](docs/es/desinstalacion-y-rollback.md).

## Plataforma validada

La aceptación física principal se realizó sobre Ubuntu 26.04, una NVIDIA RTX
2060 de 6 GB y el driver propietario NVIDIA 595.91.07. UGM no afirma
compatibilidad universal con todas las GPU: NVIDIA sigue siendo una ruta más
exigente que la configuración AMD habitual dentro del ecosistema Gamescope.

## Rendimiento

UGM evita ejecutar Gamescope anidado dentro de GNOME/Mutter. En la medición de
Dragon Ball Z: Kakarot, 1080p interno → 1440p, la sesión DRM dedicada alcanzó
aproximadamente un 18–25 % más rendimiento lineal y un 25–33 % más con FSR/NIS
que las rutas nested probadas. Consulta la
[metodología completa](docs/es/rendimiento-fsr-nis-gamescope-nested-vs-drm.md).

## Problema conocido de Steam

Algunas notificaciones de Steam Big Picture muestran un rectángulo negro en la
pila NVIDIA probada. También ocurre desde el escritorio sin Gamescope y la
superficie ya llega opaca al compositor, por lo que actualmente no existe una
corrección segura dentro de UGM. Consulta
[problemas conocidos](docs/es/problemas-conocidos.md).

## Código fuente y reproducibilidad

- `src/`: runtime y helpers de UGM.
- `system/`: unidades systemd, sesión y configuración de Gamescope.
- `packaging/`: metadatos y construcción del paquete Debian.
- `gamescope/versions/3.16.30-gbm/`: patches, procedencia, build y validación.
- `tests/`: pruebas deterministas de las integraciones opcionales.

El binario Gamescope y el icono no se guardan en Git; se incluyen en el `.deb`
versionado y se verifican mediante SHA256. Consulta [SOURCE_es.md](SOURCE_es.md).

## Agradecimientos

UGM se apoya en Valve/SteamOS/Gamescope y en la infraestructura de sesión de
ChimeraOS `gamescope-session-plus`. También agradece la documentación y el
trabajo de Bazzite y de la comunidad Linux gaming. UGM es un proyecto
independiente, no afiliado ni respaldado por Valve, ChimeraOS, Bazzite, NVIDIA
o Canonical.

## Licencia

El código original de UGM usa [MIT](LICENSE). Los componentes de terceros
mantienen sus licencias; consulta [THIRD_PARTY_NOTICES_es.md](THIRD_PARTY_NOTICES_es.md).
