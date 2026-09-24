# Historial de cambios de Ubuntu Gaming Mode

[English](CHANGELOG.md) | **Español**

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
