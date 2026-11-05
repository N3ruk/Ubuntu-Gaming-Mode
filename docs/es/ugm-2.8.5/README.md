# Ubuntu Gaming Mode 2.8.5

[English](../../ugm-2.8.5/README.md) | **Español** · [Documentación](../README.md)

Paquete **2.8.5-1**, Ubuntu **26.04 amd64**. Esta página describe cambios desde
2.8.3-2. Gamescope, VRS, Bluetooth y el tema de iconos ya existentes se documentan
en el [resumen 2.8](../ugm-2.8/README.md); no son novedades de esta release.

## Arreglos de Steam

- **YouTube con mando:** sincroniza pausa/reanudación con el reproductor listo,
  conservando la posición en vez de recrear el reproductor.
- **Actualizaciones de Steam:** el botón consulta el manifiesto oficial de la rama
  seleccionada y solicita solo actualizar el cliente mediante el actualizador
  nativo. No solicita actualizaciones del SO/BIOS, cambia de rama ni reinicia Ubuntu.
  Una versión informada `0` solo se acepta si tres huellas fijadas identifican el
  cliente conocido; no se interpreta como prueba de una actualización disponible.

Ambos hooks son independientes entre sí y de Decky. Requieren **Node.js >=22** de
APT y admiten la build **1792618285** de Steam con comprobaciones adicionales de
huellas y API. Otras builds se rechazan hasta revisarlas: no se promete soporte
para todas las futuras actualizaciones de Steam.

El paquete arranca los servicios de usuario con la sesión Steam de UGM y los
detiene al salir. Antes de iniciar Steam prepara `.cef-enable-remote-debugging`
en la instalación nativa para usar CDP en `127.0.0.1:8080`. Solo retira un marcador
propio que siga sin modificaciones; los previos o alterados se conservan. Quitar
el marcador no cierra una interfaz de depuración que Steam ya haya abierto. No
parchea archivos de Steam ni lo reinicia al instalar. Un cliente abierto puede
necesitar una salida/reentrada normal para activar la depuración. Esta integración
no cubre Steam Flatpak.

La migración conserva unidades personalizadas, máscaras y drop-ins y los avisa
como conflictos; solo retira unidades antiguas exactas de los instaladores del proyecto.

## UGM Options

Una entrada opcional **UGM Options 1.0.0** contiene cuatro paneles separados:

| Panel | Función |
| --- | --- |
| Sharp Filter | Recupera las opciones NIS/FSR y habilita el modo nitidez de QAM para funcionar con SGSR en Gamescope compatible. |
| Downsampling | Aprovecha el parche existente de Gamescope UGM para activar un filtro de nitidez regulable cuando la resolución interna supera la del monitor. |
| Video Output | Muestra pantallas detectadas y selecciona la salida de vídeo de Gaming Mode; sin controles de audio, resolución ni frecuencia. |
| Non-Steam | Añade aplicaciones Linux instaladas o ejecutables elegidos a Steam, revisando sus opciones y con Proton opcional para EXE Windows. |

Sharp Filter y Downsampling mantienen sus motores y controles existentes: agruparlos
no añade algoritmos de renderizado. Video Output conserva comprobaciones de EDID y
conector; cada GPU/dock requiere prueba física. Non-Steam evita duplicados y relee
la operación, pero añadir un acceso no instala el juego ni garantiza compatibilidad
Proton. Su consulta en disco solo lee accesos de la cuenta activa; no escribe VDF
ni fuerza reinicios de Steam.

El maestro separa procesos: solo Video Output conserva root. Sharp, Downsampling
y Non-Steam usan el usuario Decky antes de importar código. Se mantienen las
carpetas de ajustes originales de cada módulo.

## Instalación y actualización

Sigue la [guía de instalación](../instalacion.md). Decky Loader y UGM Options
tienen preguntas opcionales separadas, desactivadas en instalaciones nuevas, con
explicación breve de cada función en inglés/español. Una selección antigua puede
preseleccionar el maestro solo si su nueva pregunta aún no se ha contestado.

Al aceptar UGM Options prepara el maestro verificado fuera del árbol de carga,
detiene Decky brevemente y retira los cuatro plugins independientes detectados,
incluso carpetas renombradas identificadas por identidad. Conserva su código en
`~/homebrew/ugm-options-migration/`, fuera de `plugins/`, y respeta ajustes y otros
plugins. Si falla la sustitución/reinicio, restaura las carpetas previas. No arranca
un servicio antes detenido. Rechazar no elimina los independientes. Identidades
desconocidas, enlaces y maestros modificados se conservan con aviso. No elimina
un `output-manager` ajeno y evita un segundo escritor de vídeo.

Desinstalar UGM no desinstala Decky ni borra los ajustes de usuario.
[Fuentes/build de Decky](../../../integrations/decky/ugm-options/README_ES.md).

## Diagnóstico y límites de validación

`sudo ubuntu-gaming-mode-doctor` revisa payload frente a dpkg, Node.js, enlace de
unidades UGM, estado reciente de hooks con UGM activo, conflictos locales, maestro
y duplicados. Es de solo lectura: no conecta a CDP, busca actualizaciones, carga
plugins, cambia pantalla ni arranca/detiene servicios. Hooks parados fuera de UGM
y plugin opcional ausente son estados válidos.

La verificación automatizada cubre contratos, ciclo y limpieza del marcador,
aislamiento/navegación, migración transaccional, staging e integridad del paquete.
El maestro local se registró en Steam/Decky real y los cuatro módulos respondieron
RPC de lectura. Pruebas humanas anteriores confirmaron YouTube y búsquedas de
actualizaciones. No equivalen a probar una sesión completa con este DEB instalado.

Pendientes: navegación real con mando, cambio físico de pantalla, lanzamiento/
Proton/persistencia Non-Steam entre sesiones y una actualización completa a un
cliente Steam realmente más reciente. No se prometen FPS ni hardware universal.
[Verificación de la release](../../ugm-2.8.5/VERIFICATION.md).
