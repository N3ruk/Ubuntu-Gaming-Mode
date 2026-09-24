# Instalar Ubuntu Gaming Mode 1.0.0-3 en Ubuntu 26.04

[English](../installation.md) | **Español**

## Requisitos

Ubuntu Gaming Mode 1.0.0-3 está empaquetado para **Ubuntu 26.04 amd64**.

El paquete incluye el runtime custom de Gamescope validado utilizado por UGM y declara mediante APT sus dependencias de ejecución.

## Verificar la release

Descarga desde la misma GitHub Release:

- `ubuntu-gaming-mode_1.0.0-3_amd64.deb`
- `SHA256SUMS`

Verifica el paquete antes de instalarlo:

```bash
sha256sum -c SHA256SUMS
```

Resultado esperado:

```text
ubuntu-gaming-mode_1.0.0-3_amd64.deb: OK
```

El SHA256 del paquete GOLD es:

```text
959afa06fd87eeeba8ffc1d4a3a6c965f9fa902d5678d43597b2ce7b20dca797
```

## Instalar Ubuntu Gaming Mode

```bash
sudo apt install ./ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

Durante una primera instalación UGM:

1. detecta el usuario de escritorio y la sesión Ubuntu objetivo;
2. crea un snapshot inmutable del estado anterior a UGM;
3. instala el runtime dedicado de Gamescope y la integración de sesión;
4. configura el estado de GDM/AccountsService necesario para UGM;
5. crea el `managed-state` de la versión instalada;
6. ejecuta automáticamente `ubuntu-gaming-mode-doctor`.

El instalador **no reinicia GDM y no cierra la sesión de escritorio actual**.

## Resultado esperado del diagnóstico

Para la configuración 1.0.0-3 validada:

```text
OK:       54
Warnings: 0
Fallos:   0
```

El diagnóstico puede ejecutarse manualmente:

```bash
sudo ubuntu-gaming-mode-doctor
```

## Entrar en Gaming Mode

Desde Ubuntu Desktop, abre:

**Volver a Gaming Mode**

UGM solicita confirmación antes de cerrar la sesión de escritorio.

El siguiente login de GDM entra en la sesión gestionada `steam-gaming-mode`.

## Actualizar desde 1.0.0-2

1.0.0-3 se probó explícitamente como actualización in-place desde 1.0.0-2:

```bash
sudo apt install ./ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

La actualización conserva el snapshot original inmutable y actualiza únicamente el estado gestionado.

También corrige el problema de permisos de 1.0.0-2 por el que `/etc/ubuntu-gaming-mode` podía crearse como `0700 root:root`. En 1.0.0-3 se crea explícitamente como `0755 root:root`, mientras que `session.conf` permanece en `0644 root:root`.
