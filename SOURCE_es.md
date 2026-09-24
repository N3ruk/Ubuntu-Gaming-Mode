# Código fuente, estructura y reproducibilidad de Ubuntu Gaming Mode

[English](SOURCE.md) | **Español**

Los archivos de este repositorio están organizados a partir del paquete **Ubuntu Gaming Mode 1.0.0-3 GOLD** validado.

## Estructura de directorios

- `src/`: scripts de runtime y helpers de sesión de UGM.
- `system/`: unidades systemd, entradas de escritorio/sesión y configuración de la sesión Gamescope.
- `packaging/DEBIAN/`: archivo de control y scripts de mantenimiento de Debian.
- `packaging/build-deb.sh`: ensambla un paquete Debian a partir de los archivos del repositorio.
- `gamescope/`: información sobre el runtime custom de Gamescope validado.
- `docs/`: documentación de instalación, rollback, arquitectura y rendimiento.

## Entradas binarias que no se almacenan en Git

El paquete 1.0.0-3 contiene dos assets binarios que no se guardan dentro del historial del código fuente:

1. el ejecutable custom de Gamescope;
2. el icono PNG de UGM.

El `.deb` GOLD distribuido mediante Releases contiene ambos.

El script de build requiere por tanto:

```bash
GAMESCOPE_BIN=/ruta/a/gamescope \
UGM_ICON=/ruta/a/ubuntu-gaming-mode.png \
bash packaging/build-deb.sh
```

Para UGM 1.0.0-3, `build-deb.sh` rechaza un binario de Gamescope cuyo SHA256 no coincida con el validado.

## Estado de reproducibilidad

El repositorio puede utilizarse como base de código para futuras revisiones del paquete UGM, pero **no se afirma que 1.0.0-3 pueda reproducirse byte a byte únicamente a partir de Git**.

El binario de Gamescope incluido con 1.0.0-3 fue modificado durante el desarrollo de UGM para dos requisitos específicos del proyecto:

- funcionamiento correcto a 4K en el entorno UGM validado;
- integración con el plugin de Decky Loader [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) mediante un pequeño conector utilizado por el flujo de NIS.

El árbol de código fuente modificado de Gamescope y el conjunto exacto de parches se eliminaron después de generar el binario validado. Como esos fuentes ya no existen, este repositorio **no afirma conocer los cambios exactos** y no intentará recrearlos de memoria.

El binario actual de 1.0.0-3 y su SHA256 son por tanto la referencia canónica. Si una futura versión de UGM necesita una versión más reciente de Gamescope, el trabajo necesario para 4K y la integración Decky/NIS se volverá a implementar sobre el nuevo código upstream y se documentará en ese momento.
