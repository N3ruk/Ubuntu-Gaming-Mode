# Despertar por mando Bluetooth (UGM 2.5)

## Alcance demostrado

En el equipo de referencia MSI MPG Z390 GAMING PLUS (MS-7B51), con Bluetooth Intel USB `8087:0aaa` (`btusb`), Linux 7.0.0-38, BlueZ 5.85 y dos DualShock 4 emparejados, un mando consiguió despertar el PC desde suspensión S3 (`deep`). La BIOS tenía habilitadas las opciones de despertar por PCIe y USB. Los tres nodos de la ruta (adaptador USB, hub USB y controlador PCI XHCI) estaban en `power/wakeup=enabled`; ACPI XHC figuraba habilitado. Esta observación valida **esa combinación**, no cualquier placa, adaptador ni firmware.

## Inventario en instalación

`ugm-bt-wake probe` informa:

- modos `/sys/power/mem_sleep`, placa y adaptadores Bluetooth USB con driver `btusb`, VID:PID y controlador PCI;
- valores actuales de `power/wakeup` de adaptador, hub y XHCI, y estado ACPI de la ruta;
- disponibilidad de BlueZ/D-Bus, Python D-Bus, GTK y Polkit; mandos HID emparejados, de confianza y con `Device1.WakeAllowed`;
- `candidate` si existe ruta USB y suspensión `deep`; `eligible` si además está el software; `ready_to_test` si el estado actual permite hacer una prueba; `validated` queda siempre `false` sin prueba física registrada.

El estado `candidate` no implica que la BIOS permita wake. Debconf muestra una nota cuando detecta un HCI USB y pregunta si se desea activar Bluetooth y mostrar la app opcional en el menú. Ningún mando se autoriza automáticamente. El campo `missing` enumera componentes ausentes.

Para una instalación limpia se entrega **un único `.deb`**. Instálelo directamente con `sudo apt install ./ubuntu-gaming-mode_2.5.0-5_amd64.deb` desde una terminal interactiva. La app está integrada en ese paquete; aceptar la pregunta de Debconf publica su lanzador, rechazarla lo mantiene oculto. BlueZ y las dependencias exclusivas de la app son paquetes recomendados: APT intenta instalarlos, pero UGM se configura aunque falten. Si se acepta la app y BlueZ sigue ausente, muestra un aviso. APT instala normalmente las recomendaciones; con `--no-install-recommends` o `dpkg -i` no lo hace. Si se acepta la app y BlueZ está instalado, UGM habilita y arranca `bluetooth.service`; si falla, lo avisa sin cancelar UGM. Si se rechaza la app, no cambia el servicio. La opción está desactivada por defecto y puede cambiarse después con `sudo dpkg-reconfigure ubuntu-gaming-mode`. En instalaciones no interactivas se usan los valores predeterminados. Para inventariar sin instalar, ejecute `ugm-bt-wake probe` en un equipo con UGM instalado.

## App y persistencia

La app «UGM · Despertar por mando Bluetooth» permite activar o desactivar cada mando HID emparejado. Usa autorización Polkit para cambiar `Device1.WakeAllowed` y la ruta de wake; guarda el estado anterior por mando y nodo en `/var/lib/ubuntu-gaming-mode/bt-wake.json`. El servicio `ugm-bt-wake-apply.service` reaplica la ruta USB tras arrancar si hay mandos seleccionados. En eliminación/purge, se intenta restaurar solo valores aún coincidentes con la última escritura de UGM; los conflictos externos se preservan e informan.

Prueba de aceptación: seleccionar un mando en la app, reiniciar, comprobar que `ugm-bt-wake probe` muestra la ruta habilitada, suspender manualmente en `deep`, pulsar PS y verificar que el equipo reanuda sin usar el botón físico. Repetir con el mando deshabilitado y con un segundo mando, y probar eliminación/reinstalación.

## ¿Puede encender desde apagado?

Esta implementación cubre la suspensión S3, no el apagado S5 ni G3. En S5 BlueZ y el sistema operativo no están ejecutándose; el simple `WakeAllowed=yes` no basta. Algunas plataformas admiten wake desde S4/S5 por USB con alimentación de standby, firmware y dispositivo capaces de generar la señal, pero eso exige una prueba específica de hardware y no se puede prometer para este Intel/DS4. Desde G3 (sin alimentación de standby) no es posible por Bluetooth sin hardware externo.
