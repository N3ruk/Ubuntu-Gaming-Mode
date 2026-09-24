# Desinstalar Ubuntu Gaming Mode y restaurar el sistema con rollback

[English](../uninstall-and-rollback.md) | **Español**

UGM registra el estado que existía antes de la primera instalación y lo utiliza como baseline de rollback.

## Purge completo

Para eliminar completamente Ubuntu Gaming Mode:

```bash
sudo apt purge ubuntu-gaming-mode
```

Un purge validado de 1.0.0-3 restaura el baseline original y elimina el directorio de estado de UGM.

En el sistema limpio de referencia esto restauró:

- la sesión de AccountsService a `ubuntu`;
- el estado original de GDM;
- la eliminación de `/etc/ubuntu-gaming-mode`, ya que no existía antes de UGM;
- la eliminación de `/var/lib/ubuntu-gaming-mode`;
- la eliminación de los archivos gestionados por el paquete UGM.

## Snapshot original

La primera instalación crea un snapshot inmutable del estado original en:

```text
/var/lib/ubuntu-gaming-mode/original-state
```

Las actualizaciones conservan este baseline en lugar de sustituirlo por el estado de la versión anterior de UGM.

El snapshot incluye un manifest de integridad y un marcador de finalización.

## Estado gestionado

La versión de UGM instalada se registra por separado en:

```text
/var/lib/ubuntu-gaming-mode/managed-state
```

Esto permite al paquete diferenciar entre:

- el estado original del sistema;
- el estado gestionado por la versión de UGM actualmente instalada.

## Importante

No modifiques manualmente los valores de autologin o sesión de GDM gestionados por UGM salvo que tengas previsto repararlos después. Reinstalar o actualizar UGM puede sobrescribir valores gestionados para restaurar una configuración válida.

Para ejecutar un diagnóstico antes de eliminar el paquete:

```bash
sudo ubuntu-gaming-mode-doctor
```
