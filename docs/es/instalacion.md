# Instalar Ubuntu Gaming Mode 2.0.0-1 en Ubuntu 26.04

[English](../installation.md) | **Español**

## Requisitos

Ubuntu Gaming Mode 2.0.0-1 está empaquetado para **Ubuntu 26.04 amd64**.

El paquete incluye el runtime custom de Gamescope validado utilizado por UGM y declara mediante APT sus dependencias de ejecución.

## Verificar la release

Descarga desde la misma GitHub Release:

- `ubuntu-gaming-mode_2.0.0-1_amd64.deb`
- `SHA256SUMS`

Verifica el paquete antes de instalarlo:

```bash
sha256sum -c SHA256SUMS
```

Resultado esperado:

```text
ubuntu-gaming-mode_2.0.0-1_amd64.deb: OK
```

El SHA256 de la release es:

```text
48779954b0bc4518d2fadeb29ba636e370c84a143317f19475b4cf3f5dea369f
```

## Instalar Ubuntu Gaming Mode

```bash
sudo apt install ./ubuntu-gaming-mode_2.0.0-1_amd64.deb
```

Durante una primera instalación UGM:

1. detecta el usuario de escritorio y la sesión Ubuntu objetivo;
2. crea un snapshot inmutable del estado anterior a UGM;
3. instala el runtime dedicado de Gamescope y la integración de sesión;
4. configura el estado de GDM/AccountsService necesario para UGM;
5. crea el `managed-state` de la versión instalada;
6. ejecuta automáticamente `ubuntu-gaming-mode-doctor`.

El instalador **no reinicia GDM y no cierra la sesión de escritorio actual**.

## Botón físico estilo consola (opcional)

La versión 2.0.0-1 puede activar durante la instalación un puente para que una
pulsación corta del botón físico use el flujo nativo de suspensión de Steam.
La opción está **desactivada por defecto** y solo captura el botón mientras la
sesión Steam Gaming Mode gestionada por UGM está activa. En Ubuntu Desktop el
comportamiento del botón permanece intacto.

El daemon del sistema solo captura y valida el botón. La URI fija de Steam se
ejecuta mediante `ugm-steam-shortpowerpress.service` en el `systemd --user` de
la sesión, evitando que el launcher de Steam herede el sandbox del daemon.

La elección puede cambiarse posteriormente de forma reversible:

```bash
sudo dpkg-reconfigure ubuntu-gaming-mode
```

## Gamescope de UGM como comando global (opcional)

El instalador detecta si ya existe un comando `gamescope` y permite publicar el
runtime validado incluido en UGM mediante este enlace:

```text
/usr/local/bin/gamescope -> /usr/lib/ubuntu-gaming-mode/gamescope
```

La opción está desactivada por defecto. No desinstala ni modifica el paquete
Gamescope de Ubuntu: `/usr/local/bin` tiene normalmente prioridad sobre
`/usr/bin`. UGM conserva el estado anterior del enlace o archivo y lo restaura
al desactivar la opción o desinstalar el paquete. También puede cambiarse con
`sudo dpkg-reconfigure ubuntu-gaming-mode`.

## Resultado esperado del diagnóstico

Una instalación válida debe terminar con cero fallos. El número de comprobaciones
puede variar según el hardware y las opciones seleccionadas:

```text
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

## Actualizar desde 1.0.0-3

El paquete conserva el snapshot original de UGM al actualizar desde 1.0.0-3:

```bash
sudo apt install ./ubuntu-gaming-mode_2.0.0-1_amd64.deb
```

La actualización conserva el baseline inmutable y renueva únicamente el estado
gestionado por la versión instalada. Antes de continuar revisa cualquier aviso
del instalador y ejecuta Doctor al finalizar.
