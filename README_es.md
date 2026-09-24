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

## NVIDIA: Gamescope DRM 4K, HDR y VRR validados en una RTX 2060

UGM 1.0.0-3 se desarrolló y superó su aceptación física sobre una **NVIDIA GeForce RTX 2060 de 6 GB** utilizando el driver propietario **NVIDIA 595.91.07** en Ubuntu 26.04.

La sesión dedicada Gamescope DRM/KMS validada incluye:

- salida 4K;
- HDR;
- VRR / Adaptive Sync;
- Steam Overlay;
- soporte de mando;
- MangoApp / MangoHud;
- escalado FSR y NIS;
- cambio Desktop ↔ Gaming Mode.

NVIDIA continúa siendo una ruta más exigente dentro del ecosistema SteamOS/Gamescope que la configuración AMD habitual. Las referencias siguientes se incluyen como contexto técnico, **no como crítica hacia Bazzite o ChimeraOS**. Ambos proyectos han realizado aportaciones muy importantes al gaming en Linux y su documentación es especialmente valiosa precisamente porque expone con transparencia las limitaciones de hardware y sus workarounds:

- Gamescope upstream admite el driver propietario de NVIDIA, pero exige un driver suficientemente reciente y soporte DRM KMS/modesetting ([README de Gamescope](https://github.com/ValveSoftware/gamescope));
- Bazzite clasifica oficialmente Steam Gaming Mode con NVIDIA como soportado con **limitaciones importantes** frente a AMD. Su documentación explica que activar **GPU accelerated rendering in web views** puede mejorar el rendimiento de la interfaz de Steam en NVIDIA, aunque también advierte de posibles artefactos gráficos graves ([compatibilidad de hardware](https://docs.bazzite.gg/Gaming/Hardware_compatibility_for_gaming/), [quirks de Gaming Mode](https://docs.bazzite.gg/Handheld_and_HTPC_edition/quirks/));
- ChimeraOS documentó históricamente las dificultades de NVIDIA con Wayland/Gamescope, retiró temporalmente el soporte NVIDIA y posteriormente volvió a ofrecer soporte oficial para GTX serie 16 y posteriores, manteniendo la advertencia de un rendimiento pobre de la interfaz de Steam en NVIDIA. Su documentación de troubleshooting recomienda la aceleración GPU de las webviews de Steam para reducir ese lag ([notas de versión](https://github.com/ChimeraOS/chimeraos/wiki/Release-Notes), [troubleshooting](https://github.com/ChimeraOS/chimeraos/wiki/Troubleshooting));
- un reporte upstream de Gamescope de 2026 documenta **corrupción de scan-out 4K con NVIDIA**: aparecen columnas/bandas verticales con contenido del framebuffer desplazado a 3840×2160 @ 30/60/120, mientras 1080p y 1440p permanecen limpios. El problema se reprodujo en RTX 4080 SUPER y RTX 4070 y también aparece con contenido estático y con force-composition activado ([Gamescope issue #2309](https://github.com/ValveSoftware/gamescope/issues/2309)).

En el sistema de referencia de UGM con RTX 2060 se ha validado físicamente una **interfaz Steam Gamepad UI fluida tanto a 60 FPS como a 120 FPS** dentro de Gaming Mode, además de las pruebas 4K/HDR/VRR indicadas arriba. Durante esa aceptación no se observaron problemas de lag en la UI ni artefactos gráficos que rompieran los juegos.

### Corrupción histórica de scan-out 4K en NVIDIA resuelta en la build validada de UGM

Durante el desarrollo de UGM, el sistema de referencia con RTX 2060 sufrió anteriormente un fallo grave de presentación 4K sobre DRM que visualmente coincide con la misma familia de corrupción que ahora describe el issue #2309 de Gamescope:

- la imagen 4K aparecía cortada en varias secciones;
- partes del framebuffer se mostraban colocadas en zonas de la pantalla que no correspondían;
- aparecía corrupción intensa de color azul/cian, rosa/magenta y morado por la imagen;
- el fallo estaba asociado a la ruta Gamescope DRM a 4K.

La build actual de Gamescope incluida en UGM 1.0.0-3 ya no reproduce ese problema en la RTX 2060 validada: la salida 4K está estabilizada, incluidas las pruebas con HDR, VRR y la interfaz de Steam.

Como el árbol fuente modificado de Gamescope se eliminó posteriormente, UGM no puede demostrar qué cambio exacto eliminó la corrupción ni afirmar que resuelva el issue upstream todavía abierto en otras GPU NVIDIA. La afirmación verificable es más concreta: **la corrupción de scan-out NVIDIA a 4K encontrada durante el desarrollo de UGM fue superada y la configuración RTX 2060 de referencia es actualmente estable a 4K**.

UGM **no** afirma compatibilidad universal con NVIDIA ni que esos problemas de upstream o de otras distribuciones estén resueltos para todas las GPU. La afirmación probada es más concreta: el sistema de referencia con RTX 2060 ejecuta correctamente una sesión Gamescope DRM independiente a 4K con HDR, VRR, interfaz Steam fluida a 60/120 FPS y las funciones de Steam Gaming Mode indicadas arriba.

Para usuarios de NVIDIA que ya utilizan Ubuntu, UGM ofrece un **modelo de integración diferente** al de distribuciones gaming dedicadas como SteamOS, Bazzite o ChimeraOS: añade una sesión Gamescope tipo consola independiente conservando el escritorio Ubuntu y la pila de drivers existente. Es una diferencia de enfoque e integración, no una afirmación de que UGM sustituya o sea superior a esos proyectos.

La build de Gamescope incluida se adaptó durante el desarrollo para la ruta 4K validada y la integración con Sharp Filter Selector. Su árbol fuente modificado se eliminó posteriormente, por lo que no se reconstruyen ni se atribuyen de memoria los cambios exactos realizados en Gamescope.

## Agradecimientos

Ubuntu Gaming Mode se apoya en el trabajo de toda la comunidad de gaming en Linux. **UGM no existiría en su forma actual sin los proyectos siguientes.**

Un agradecimiento especial a **Valve, SteamOS y el equipo de Steam Deck**. SteamOS estableció el modelo de sesión de juego dedicada en el que se inspira UGM, y el trabajo open source de Valve sobre el compositor evolucionó desde `steamos-compositor` hasta **Gamescope**, la tecnología central que hace posible el Gaming Mode DRM/KMS de UGM. Steam Deck y SteamOS también constituyen la referencia práctica de una interfaz Gaming Mode orientada a mando que convive con un escritorio Linux normal.

Un agradecimiento especialmente importante a **ChimeraOS**. Su proyecto **gamescope-session-plus** está basado explícitamente en el trabajo de Valve con Gamescope/Steam Deck, está diseñado para poder utilizarse más allá de ChimeraOS y aportó tanto documentación como una base técnica fundamental para el runtime de sesión de UGM. UGM adapta ese trabajo a su entorno Ubuntu/GDM/systemd y conserva la atribución upstream y la licencia MIT correspondientes.

Gracias también a **Bazzite** por su amplia implementación de Steam Gaming Mode y por su documentación sobre una gran variedad de hardware. Su forma transparente de documentar las limitaciones y workarounds de NVIDIA ha sido una referencia útil para validar y explicar el comportamiento NVIDIA de UGM.

UGM es un proyecto independiente y no está afiliado ni respaldado por Valve, ChimeraOS, Bazzite, NVIDIA o Canonical. Las comparaciones de este README pretenden documentar comportamiento probado y contexto técnico, no restar valor al trabajo de los proyectos que han hecho posible este ecosistema.

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
- [Notas de la release v1.0.0-3 GOLD](docs/releases/v1.0.0-3.md)
- [Avisos de terceros](THIRD_PARTY_NOTICES_es.md) · [English](THIRD_PARTY_NOTICES.md)

## Estado

1.0.0-3 es la primera versión candidata promovida a **GOLD** después de validar instalación limpia, actualización, purge/rollback y el ciclo físico Desktop ↔ Gaming Mode.

## Licencia

El código original del proyecto UGM se publica bajo la [Licencia MIT](LICENSE).

Los componentes de terceros conservan sus propias licencias y avisos de copyright. Consulta [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) y el [resumen en español](THIRD_PARTY_NOTICES_es.md), incluyendo el aviso MIT de ChimeraOS gamescope-session y la licencia BSD 2-Clause de Gamescope.
