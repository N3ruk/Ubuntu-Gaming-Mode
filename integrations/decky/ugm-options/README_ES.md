# UGM Options

**1.0.0, incluido en UGM 2.8.5.** Una entrada en Decky, cuatro paneles originales:
Sharp Filter, Downsampling, Video Output y Non-Steam. [English](README.md).

Solo añade portada y navegación con botón de regreso y B. En Non-Steam, B respeta
primero la navegación propia de sus subpantallas. No mezcla ajustes ni incorpora
«Aplicar todo», perfiles, nuevos automatismos o reinicios de Steam.

Los cuatro motores y bundles originales se conservan byte a byte en modules/.
El frontend del maestro adapta únicamente el nombre/conexión Decky y el transporte
de llamadas. manifest.json registra todas las huellas. Cada motor mantiene su
proceso, variables, tareas y configuración. Cambiar de sección desmonta el panel,
no el motor; la descarga de Decky llama a cada _unload original.

## Permisos

El broker y Video Output tienen root por los requisitos originales del vídeo.
Los procesos Sharp, Downsampling y Non-Steam pierden privilegios **antes de
importar código**, con UID/GID/grupos del usuario de Decky. Non-Steam conserva su
rechazo a root. No hay shell privilegiada, puerto nuevo ni servicio adicional.
La API admite solo métodos registrados por módulo; un error no bloquea los otros.
No se reintentan escrituras cuyo resultado haya quedado indeterminado.

## Ajustes y migración

Se reutilizan settings/sharp-filter-selector/, settings/ugm-downsampling-sharpener/,
settings/ugm-video-output/ y settings/ugm-non-steam/. No se convierten ni reinician
ajustes. Una instalación nueva conserva los valores iniciales originales.

El instalador UGM ofrece Decky Loader y este maestro; ya no instala los módulos
como entradas independientes. Al aceptar UGM Options, ugm-decky-integration valida
el payload completo, prepara el maestro fuera de plugins/, detiene Decky y retira
las cuatro carpetas independientes detectadas (también por su nombre de plugin).
Conserva su código en homebrew/ugm-options-migration/, fuera del árbol de carga;
no toca settings/ ni otros plugins. Instala el maestro con código root-owned y
permisos de lectura/ejecución explícitos. Reinicia Decky solo si estaba activo.
Si la sustitución o el reinicio falla, restaura las carpetas anteriores.

Las rutas enlazadas o identidades desconocidas se conservan y se avisan; un maestro
modificado no se sobrescribe. El broker bloquea módulos con copia independiente
presente; vídeo también detecta output-manager, que el migrador no elimina.
Reversión manual: detener Decky, retirar el maestro y restaurar las carpetas
archivadas, conservando settings/. El ensamblado por sí solo no instala nada.

## Compilar y comprobar

Ver comandos y requisitos en [README.md](README.md). ZIP reproducible en release/.
No genera DEB ni modifica Gamescope o hooks. La receta UGM incluye solo el maestro. Nuevos
módulos se registran mediante modules.json sin añadir entradas en Decky.

Instalación local, registro en Steam/Decky real y consultas RPC de lectura de los
cuatro módulos verificados. Pendientes mando, cambio físico de pantalla,
lanzamientos y persistencia entre sesiones. [Guía UGM vigente](../../../docs/es/ugm-2.8.5/README.md).
