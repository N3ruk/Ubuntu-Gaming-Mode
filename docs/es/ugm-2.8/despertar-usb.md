# Despertar por mando Bluetooth en UGM 2.8

[English](../../ugm-2.8/usb-wake.md) | **Español** · [Resumen 2.8](README.md)

Al aceptar la opción de despertar Bluetooth en una **instalación nueva de
2.8**, UGM aplica una política de denegación por defecto a los dispositivos
USB físicos que admitan `power/wakeup`. Mantiene habilitados el adaptador
`btusb` y su ruta de hub/controlador PCI necesaria para despertar por
Bluetooth. Los ratones, teclados, receptores y mandos USB no autorizados
quedan con `power/wakeup=disabled`; los mandos Bluetooth emparejados
comienzan con `Device1.WakeAllowed=no`, incluso si ya estaban emparejados
antes de instalar UGM. La app autoriza cada mando Bluetooth por separado y
no cambia los permisos de otros USB.

El servicio de arranque reaplica la política. Una regla udev lanza una
aplicación puntual para cada USB conectado después de la instalación; otra
lanza la preparación de la ruta si se conecta o restablece un adaptador
`btusb`. Los hubs raíz no se deniegan, porque pueden formar parte de la ruta
compartida. La política registra el valor anterior, la identidad y el
arranque de cada dispositivo modificado; al desactivar la opción o
desinstalar, intenta restaurarlo solo si nadie cambió ese valor después.

**Migración:** actualizar desde UGM 2.5 no activa esta política ni revoca
permisos existentes. La selección de mandos y la ruta de wake anterior se
conservan. La política estricta queda reservada a instalaciones nuevas de
2.8, también si se habilita la opción más tarde mediante `dpkg-reconfigure`.

La regla actúa sobre el permiso USB del kernel. La BIOS/UEFI, ACPI, otros
buses y reglas de terceros pueden ofrecer fuentes de despertar adicionales;
la compatibilidad con S3 y el aislamiento entre puertos han de comprobarse
con una suspensión real. Esto no modifica la función del botón físico ni
promete encendido desde S5.
