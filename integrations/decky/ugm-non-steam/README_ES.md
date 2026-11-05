# UGM Añadir contenido Non-Steam

[English](README.md)

**Incluido como panel Non-Steam de UGM Options en UGM 2.8.5.**
Adaptación de Add Non-Steam Games con origen fijado y licencia BSD conservada.

## Flujo

1. **Aplicaciones instaladas** o **Buscar ejecutable**.
2. Seleccionar el programa, con filtro y listas paginadas.
3. Revisar nombre, destino, carpeta de inicio, argumentos y compatibilidad.
4. **Añadir a Steam**.
5. **Abrir ficha**, **Comprobar guardado en disco** o **Deshacer este acceso**.

El catálogo contempla entradas XDG Linux, Flatpak y Snap. El explorador admite
binarios Linux, AppImage, scripts con intérprete y EXE Windows. Permite rutas
con espacios, carpetas ocultas y unidades ya montadas. Para EXE se exige elegir
Proton explícitamente. Si falta permiso de ejecución en un programa Linux,
se ofrece habilitarlo solo para tu usuario mediante un botón explícito.

El borrador se conserva al cerrar y reabrir QAM durante la carga del plugin.
Cambia de cuenta Steam y se vacía. No se guardan rutas privadas en localStorage.

## Qué comprueba y qué no

- Previene duplicados por destino y argumentos en la biblioteca viva.
- Relee nombre, ejecutable, carpeta, argumentos y Proton solicitado. Si Steam
  omite datos o ignora cambios, muestra comprobación incompleta.
- Comprueba en modo solo lectura el VDF de la cuenta activa indicada por Steam;
  no busca la última cuenta usada ni modifica shortcuts.vdf.
- Guardado en disco y alta en Steam son estados diferentes. No garantiza inicio
  del juego, compatibilidad real con Proton ni persistencia tras reiniciar hasta
  probar un ciclo real. El VDF tampoco demuestra la configuración de Proton.
- Deshacer requiere confirmación, solo afecta al acceso de la operación actual
  y se niega si sus datos han cambiado. No borra juegos, partidas ni prefijos.

No reinicia Steam, no sale de UGM, no modifica wrappers, no instala tiendas/juegos,
no monta unidades y no usa sudo/root. Entradas de terminal, solo DBus o con Exec
no admitido se excluyen. No ofrece MSI/BAT/CMD ni importa juegos de Heroic/Lutris.
Añadir el ejecutable de un juego de launcher no reproduce su prefijo o autenticación.

## Validación pendiente

Las pruebas automatizadas y la compilación no sustituyen la prueba con mando
real: foco/teclado, altas y propiedades, apertura/lanzamiento, Proton y guardado
tras salida limpia/reentrada a UGM. No se han probado altas ni modificado la biblioteca para validar esta release.

El ZIP independiente es de desarrollo; el instalador UGM solo distribuye el maestro.
El panel mantiene motor y controles, con worker sin root. Registro y RPC de lectura
en Decky real verificados; lanzamientos y persistencia siguen pendientes.
[Guía UGM](../../../docs/es/ugm-2.8.5/README.md).

Construcción y arquitectura: [README.md](README.md). Créditos: [NOTICE.md](NOTICE.md).
