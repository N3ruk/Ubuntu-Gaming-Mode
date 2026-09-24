# Gamescope custom incluido en Ubuntu Gaming Mode

[English](README.md) | **Español**

Ubuntu Gaming Mode 1.0.0-3 GOLD incluye dentro del paquete un binario **custom** y validado de Gamescope.

No es una build upstream sin modificar. Durante el desarrollo de UGM se modificó para:

- funcionar correctamente con la configuración 4K validada de Gaming Mode;
- interoperar con el plugin de Decky Loader [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector) mediante un pequeño conector utilizado por la integración NIS.

Runtime validado:

```text
gamescope 3.16.28-3-g0d07f6e
```

SHA256:

```text
e43f0737287b2812d0c34a43c638131b3656058e1666d55228ad8acfe59d616c
```

Ruta instalada:

```text
/usr/lib/ubuntu-gaming-mode/gamescope
```

El binario compilado se distribuye dentro del `.deb` de la release y no se almacena en el historial Git.

## Estado de reproducibilidad

El repositorio de UGM 1.0.0-3 publica los archivos fuente y de configuración de UGM extraídos del paquete GOLD validado.

El árbol de código fuente modificado de Gamescope utilizado para producir el binario validado de 1.0.0-3 fue eliminado posteriormente. Por tanto, ya no pueden recuperarse de forma fiable las modificaciones exactas, el conjunto de parches ni la receta de compilación, y este repositorio **no los reconstruye de memoria**.

Para 1.0.0-3, el binario validado y el SHA256 anterior constituyen la referencia canónica de integridad.

Sí se conoce un comportamiento histórico concreto aunque se haya perdido el parche exacto: antes de la build final validada, el sistema de referencia con RTX 2060 sufría corrupción grave de scan-out DRM a 4K, con la imagen dividida en secciones desplazadas y fuerte corrupción azul/cian, rosa/magenta y morada. La build actual de Gamescope de 1.0.0-3 ya no reproduce ese fallo en el sistema validado y su ruta 4K está estabilizada.

Esto se documenta deliberadamente como un resultado observado antes/después, no como una afirmación sobre el cambio interno exacto ni sobre el comportamiento universal de NVIDIA.

No se pretende hacer ingeniería inversa ni aproximar los parches perdidos únicamente para documentarlos. Cuando UGM necesite actualizar Gamescope a una versión más reciente, el comportamiento 4K necesario y la integración con Sharp Filter Selector se volverán a implementar sobre la nueva base de código y se documentarán como parte de esa futura versión.

No sustituyas este binario dentro de un paquete 1.0.0-3 sin cambiar la versión del paquete y repetir las pruebas físicas de aceptación.
