# Problemas conocidos

[English](../known-issues.md) | **Español**

## Rectángulo negro alrededor de notificaciones de Steam Big Picture

### Estado

**Limitación externa actualmente no corregible de forma segura por UGM.**

En el sistema NVIDIA de referencia, algunas notificaciones de Steam Gamepad UI
se muestran dentro de una superficie rectangular negra en lugar de conservar la
transparencia alrededor del contenido. El tamaño y la posición de la
notificación son correctos a 1080p, 1440p y 4K.

### Por qué no se atribuye actualmente a UGM

- El mismo rectángulo negro se reproduce en Steam Big Picture desde el
  escritorio normal, sin ejecutar Gamescope ni la sesión UGM.
- La superficie `notificationtoasts_uid2` capturada desde X11 ya contenía los
  márgenes negros con alpha opaco antes de la composición final de Gamescope.
- Los logs de `steamwebhelper` registran
  `GLX_EXT_texture_from_pixmap extension unavailable`, cambian del compositor
  OpenGL al compositor System y después indican que el fondo transparente
  solicitado no está soportado.
- Forzar `-cef-force-glx` no cambió el síntoma ni evitó ese fallback.

La evidencia apunta a la ruta de composición de Steam/steamwebhelper y su
interacción con la pila gráfica NVIDIA. Esto no demuestra todavía qué componente
concreto de Steam, CEF o el driver contiene el defecto, pero sí demuestra que no
es seguro presentarlo como un fallo reparable mediante el runtime UGM.

### Alcance de UGM

UGM controla la sesión Gamescope, sus argumentos y el runtime empaquetado. No
puede recuperar transparencia que Steam ya ha sustituido por píxeles negros
opacos antes de entregar la superficie al compositor. Intentar corregirla en
Gamescope implicaría heurísticas destructivas que podrían volver transparentes
elementos legítimamente negros de Steam o de los juegos.

Por este motivo UGM no modifica Gamescope para este problema. La funcionalidad
de juegos, 4K, HDR, VRR, escalado y Steam Overlay no queda afectada por esta
decisión.

### Seguimiento upstream y mitigación

- ValveSoftware/steam-for-linux
  [#13196](https://github.com/ValveSoftware/steam-for-linux/issues/13196)
  describe el mismo borde negro en Big Picture sin depender de Gamescope.
- ValveSoftware/gamescope
  [#2171](https://github.com/ValveSoftware/gamescope/issues/2171) documenta un
  síntoma parecido con NVIDIA 595, aunque en ese sistema solo aparece bajo
  Gamescope y por tanto no es idéntico a la reproducción de UGM.

No existe por ahora un workaround comunitario universal y confirmado. Como
mitigación puede reducirse o desactivarse la categoría de notificaciones en los
ajustes de Steam. El problema se reabrirá en UGM si una futura actualización de
Steam/driver lo corrige o si aparece evidencia reproducible de un componente
específico de UGM.
