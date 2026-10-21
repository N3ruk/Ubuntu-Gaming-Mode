# Historial de cambios de Ubuntu Gaming Mode

[English](CHANGELOG.md) | **Español**

## 2.8.3-2 — 2026-10-21

- Corregida la redacción del changelog del paquete Debian: el aviso del instalador está en español, no es bilingüe. Sin cambios en el funcionamiento.

## 2.8.3-1 — 2026-10-21

- Aclarado en el instalador que el plugin opcional Sharp Filter Selector de Decky enlaza «Nitidez» de QAM con SGSR cuando Gamescope lo admite; Gamescope por sí solo no hace que QAM seleccione SGSR. El plugin también permite elegir NIS o forzar FSR.
- Aclaradas la descripción del plugin y la documentación bilingüe. No cambia la lógica de renderizado SGSR.

## 2.8.2-1 — 2026-10-19

- Pregunta opcional durante la instalación para instalar y activar UGM-ICONS en el escritorio del usuario de UGM. Conserva un tema local preexistente y respeta cambios posteriores de selección; al desactivar o desinstalar, restaura la selección previa cuando corresponde. Los archivos se conservan para proteger cambios del usuario.
- El tema de iconos se incluye en el `.deb`; no requiere descarga adicional.

## 2.8.0-2

- Corrige el `umask` del instalador de Decky, permisos de `PluginLoader` y payload de ambos plugins; recarga el servicio si está activo.

## 2.8.0-1 — paquete local (sin publicar)

### Resumen de la preparación

- [Índice de novedades y pendientes de build](docs/es/ugm-2.8/README.md).
- Ya preparado: parche 0010 de Gamescope para downsampling con nitidez selectiva
  (filtro 0.4), e integración opcional de Decky Loader y dos plugins con preguntas
  independientes y desactivadas por defecto. Ver [downsampling](docs/es/ugm-2.8/downsampling.md)
  y [Decky](docs/es/ugm-2.8/decky.md). Esta incorporación VRS no modifica esas áreas.
- Primer `.deb` local **2.8.0-1** construido el 17-10-2026; sin publicación.
- Doctor ajustado al hash/versión de Gamescope 0.4, versión del estado gestionado
  y caché de inventario 2.8.0-1. Requiere root para comprobar el estado privado.
- 80 pruebas automatizadas superadas, integridad y dependencias del paquete
  verificadas. La instalación limpia de este candidato reveló un fallo de permisos de Decky, corregido en 2.8.0-2.

### Capa VRS actualizada — incorporación 2026-10-17

- Núcleo compartido AMD/NVIDIA de 32/64 bits: dispatch por instancia/dispositivo,
  tablas hash, manejo de fallos parciales y fallback conservador ante falta de memoria.
- Menor coste CPU en binds y lectura QAM por metadatos síncronos; bibliotecas
  publicadas por rename y staging completo en generaciones inmutables.
- Política SPIR-V más protectora y compatible con formas modernas demostrables,
  identificadores de shader ya observados y enlaces Graphics Pipeline Library
  (GPL)/flags2; conserva bibliotecas originales y usa variantes privadas seguras.
- Selección VRS independiente de la pantalla para una GPU única con varios
  monitores; conserva HDR/VRR y las rutas de otras capas del loader Vulkan.
- Optimización final: cuatro reservas temporales menos por lote exclusivamente
  GPL. Ahorro CPU acotado, con pequeño sobrecoste en controles no GPL; no acredita
  más FPS ni mayor cobertura por sí misma.
- QAM público 1×1/2×2 intacto; tasas 1×2/2×1, MSAA ampliado, diagnóstico y captura
  siguen siendo opciones explícitas. No se incluyen sondas de juegos ni cachés
  experimentales rechazadas. AMD sigue sin validación en GPU física.
- Se trasladan únicamente fuentes, integración VRS y pruebas, sin cambiar
  instalación local, Gamescope, Decky ni Bluetooth. No se publica en GitHub.
- Verificación en el proyecto destino: 77 tests Python, matriz CPU 32/64 bits
  y sanitizers, 60 casos GPU NVIDIA y cuatro bibliotecas idénticas al origen.
- [Detalle, validación y límites](docs/es/ugm-2.8/vrs.md);
  [manifiesto de procedencia](docs/es/ugm-2.8/vrs-import-manifest.json).

### Despertar por mandos Bluetooth y política USB

- En instalaciones nuevas que activen la opción de despertar Bluetooth, se
  deniega por defecto `power/wakeup` a los dispositivos USB físicos que lo
  admitan, excepto al adaptador `btusb`. Se mantiene habilitada su ruta
  compartida de hub/controlador necesaria para despertar por Bluetooth.
- La política se aplica a los USB ya presentes y a los que se conecten después;
  se reaplica al arrancar y al reconectar el adaptador Bluetooth. Ratones,
  teclados, receptores y mandos USB no se autorizan desde la app.
- En esa instalación nueva, los mandos Bluetooth emparejados, incluidos los
  anteriores a UGM, comienzan con `WakeAllowed=no`. Los mandos emparejados más
  tarde también requieren autorización individual en la app. Deben ser de
  confianza y exponer `WakeAllowed`.
- La actualización desde 2.5 conserva la política y los permisos existentes;
  no activa retroactivamente la denegación USB. Al desactivar la opción o
  desinstalar, UGM intenta restaurar los valores USB previos que registró,
  siempre que el dispositivo y su estado no hayan cambiado por otra vía.
- Implementación y pruebas aisladas preparadas. Pendiente construir la versión
  2.8 y validar el despertar mediante una suspensión real; el comportamiento
  final también depende del firmware y del hardware.

## 2.5.0-5 — 2026-10-12

- Conservado el Gamescope 3.16.30 GBM de 2.0.0-1 y su serie de nueve patches;
  no se ha cambiado ese binario para 2.5.
- Añadidos inventario GPU/Vulkan, opciones HDR/VRR y diagnóstico de capacidades.
- Añadida selección VRS en instalación: ruta nativa RADV para una lista
  conservadora de APU, o capas experimentales AMD/NVIDIA con permiso explícito.
  Ambas capas se generan desde fuentes C en 32 y 64 bits y se empaquetan inertes.
- Añadida app opcional de despertar por mando Bluetooth, con icono, Polkit y
  servicio, dentro del mismo paquete; BlueZ es recomendado y la opción comienza
  desactivada.
- Corregido el fallo de permisos de VRS: la sesión prepara la capa desde
  `/usr/lib/ubuntu-gaming-mode/vrs-bundled`, legible por el usuario, y mantiene
  privado `/var/lib/ubuntu-gaming-mode`. El hook anuncia VRS a Steam solo si la
  preparación de la ruta seleccionada termina correctamente.
- Pruebas automáticas: 61 superadas para el paquete; dos builds idénticas byte a byte. Publicada como estable tras la validación del mantenedor.

## 2.0.0-1 — 2026-09-30

### Cambiado

- Preparado el paquete alrededor del runtime reproducible Gamescope 3.16.30 GBM.
- Actualizadas las comprobaciones de integridad y el doctor para el runtime
  validado `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.
- Incluido el arreglo de drenaje de sesión ya presente en la rama `main` actual.
- Añadido un puente opcional y reversible para el botón físico con suspensión
  estilo consola. Está desactivado por defecto, captura pulsaciones cortas solo
  en la sesión Steam Gaming Mode validada y delega la suspensión al flujo nativo
  `steam://shortpowerpress` de Steam. El comportamiento del escritorio no cambia.
  La entrega final se ejecuta mediante una unidad `systemd --user` para que el
  launcher de Steam de 32 bits no herede el sandbox del daemon del sistema.
- Añadida una opción Debconf, desactivada por defecto, para publicar el
  Gamescope validado de UGM como comando global mediante
  `/usr/local/bin/gamescope`. No desinstala el Gamescope de la distribución,
  preserva el estado anterior y lo restaura al desactivar la opción o retirar
  UGM.

### Estado de validación

- Instalación limpia, transiciones Desktop/Gaming, botón físico, icono y Doctor
  validados en el equipo de referencia.
- Opción global de Gamescope validada con 8 pruebas deterministas de creación,
  restauración y rechazo seguro; paquete construido dos veces de forma idéntica.
- 11 pruebas del botón físico y 35 hashes internos del paquete superados.

### Conservación del código de Gamescope 3.16.30

- Añadido el patch 0009 a la serie GBM reproducible. Si no puede importarse un
  semáforo timeline del reescalado preventivo, continúa por composición normal
  en lugar de permitir que una dependencia/señal nula derribe la sesión.
- Conservado el cambio de seguridad como commit `190d2cf`, con árbol fuente final
  `5fd5ea2159236ba93defec52ffef4ee0ddfa0bc1`.
- Registrado el SHA256 del runtime validado:
  `5ddf50c78c7e2cf6bb9bc6485ccf7f5791da2d11b378bedaefd13abfc59ed256`.
- Documentada la separación entre los ocho patches GBM derivados de upstream y
  el patch de seguridad de UGM, incluida la prueba real satisfactoria del fallback.

## 1.0.0-3 — GOLD

Versión de producción promovida a GOLD después de superar la aceptación física desde una instalación limpia.

### Corregido

- Corregido un problema por el que `/etc/ubuntu-gaming-mode` podía heredar permisos restrictivos durante el setup.
- El directorio de configuración ahora se crea explícitamente como `0755 root:root`.
- `session.conf` permanece en `0644 root:root`.
- Ampliado `ubuntu-gaming-mode-doctor` para comprobar que `session.conf` puede ser leído realmente por el usuario UGM configurado.

### Validado

- Instalación limpia desde un sistema sin estado previo de UGM y sin Gamescope en `PATH`.
- Actualización de 1.0.0-2 a 1.0.0-3.
- Reparación automática durante el upgrade del problema de permisos de 1.0.0-2.
- Snapshot original inmutable conservado durante la actualización.
- El purge restaura el baseline original.
- Ciclo físico Desktop → Gaming Mode → Desktop.
- Salida 4K.
- HDR.
- VRR / Adaptive Sync.
- Steam Overlay.
- Soporte de mando.
- MangoApp / MangoHud.
- Integridad SHA256 de Gamescope y del runtime.
- Resultado final del doctor: **54 OK / 0 warnings / 0 fallos**.

### Artefacto GOLD

```text
ubuntu-gaming-mode_1.0.0-3_amd64.deb
959afa06fd87eeeba8ffc1d4a3a6c965f9fa902d5678d43597b2ce7b20dca797
```

## 1.0.0-2

Candidata de producción anterior a GOLD utilizada para la primera pasada completa de aceptación física.

La pila funcional de Gaming Mode superó las pruebas de 4K, HDR, VRR, mando y MangoHud, pero la prueba física Desktop → Gaming detectó un problema de permisos en `/etc/ubuntu-gaming-mode`. El problema quedó corregido en 1.0.0-3.
