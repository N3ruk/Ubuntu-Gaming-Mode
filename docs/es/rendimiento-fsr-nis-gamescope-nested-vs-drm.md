# Rendimiento FSR/NIS en Gamescope: modo nested frente a DRM Gaming Mode

[English](../performance/fsr-nis-nested-vs-drm.md) | **Español**

Estado: **documentado a partir de mediciones reales del proyecto**

Sistema de referencia:

- GPU: NVIDIA GeForce RTX 2060
- Escritorio: Ubuntu / GNOME / Mutter / Wayland
- Gamescope: `3.16.28-3-g0d07f6e`, build custom de UGM
- Flujo NIS: plugin de Decky Loader [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector)
- Juego: Dragon Ball Z: Kakarot
- Resolución interna: 1920×1080
- Resolución de salida: 2560×1440

## Objetivo

Este documento cubre dos puntos del proyecto:

1. comparar el rendimiento de FSR/NIS entre Gamescope ejecutado nested dentro del escritorio y Gamescope funcionando directamente mediante DRM/KMS;
2. explicar por qué el coste de rendimiento de FSR/NIS es mucho mayor dentro de GNOME/Mutter que en la sesión Gaming Mode dedicada.

## Mediciones

### Gamescope nested — backend SDL

- Linear: ~96 FPS, aproximadamente ±2 FPS
- FSR / NIS: ~88–91 FPS

Penalización aproximada del filtro:

- 96 → 91 FPS: ~5,2 %
- 96 → 88 FPS: ~8,3 %
- Penalización media observada: ~6,8 %

### Gamescope nested — backend Wayland

- Linear: ~101–102 FPS
- FSR / NIS: ~88 FPS

Utilizando ~101,5 FPS como referencia linear:

- Pérdida: ~13,5 FPS
- Penalización aproximada: ~13,3 %

### 2560×1440 nativo sin Gamescope

- ~70 FPS

Este valor **no** se utiliza para calcular el coste de FSR/NIS porque el juego está renderizando a una resolución interna distinta. Solo sirve para mostrar que renderizar a 1080p y escalar a 1440p continuó ofreciendo una ventaja de rendimiento importante en esta prueba.

### Gamescope DRM / Gaming Mode

- Linear: ~120 FPS
- FSR / NIS: ~114–117 FPS

Penalización aproximada del filtro:

- 120 → 117 FPS: ~2,5 %
- 120 → 114 FPS: ~5,0 %
- Rango típico observado: ~2,5–5 %

## Ganancia de rendimiento nested → DRM

### Linear

- SDL nested: ~96 FPS
- Wayland nested: ~101–102 FPS
- DRM Gaming Mode: ~120 FPS

Ganancia aproximada:

- DRM frente a SDL: ~+25 %
- DRM frente a Wayland/Mutter: ~+18,2 %

Rango observado: **~18–25 % más rendimiento**

### FSR / NIS

- SDL nested: ~88–91 FPS
- Wayland nested: ~88 FPS
- DRM Gaming Mode: ~114–117 FPS

Comparaciones razonables entre extremos:

- 88 → 114 FPS: ~+29,5 %
- 88 → 117 FPS: ~+33,0 %
- 91 → 114 FPS: ~+25,3 %
- 91 → 117 FPS: ~+28,6 %

Rango observado: **~25–33 % más rendimiento**

Estas cifras pertenecen a esta prueba concreta y no deben interpretarse como una ganancia universal para todos los juegos, GPU o resoluciones.

## Por qué existe esta diferencia

Gamescope tiene dos escenarios de funcionamiento fundamentalmente diferentes.

### DRM dedicado / Gaming Mode

Ruta simplificada:

```text
Juego
  -> Gamescope
  -> FSR/NIS cuando está activado
  -> DRM/KMS
  -> Pantalla
```

En este modo Gamescope posee la sesión DRM y presenta directamente sobre la ruta de pantalla. Puede evitar el compositor de escritorio en la cadena final de presentación y aprovechar direct flips o hardware planes cuando están disponibles.

Es el escenario más cercano al Gaming Mode de SteamOS.

### Nested dentro de GNOME/Mutter

Ruta simplificada:

```text
Juego
  -> Gamescope
  -> FSR/NIS
  -> superficie Wayland/SDL de Gamescope
  -> Mutter
  -> DRM/KMS
  -> Pantalla
```

Aquí Gamescope no controla directamente la ruta final hacia la pantalla.

Intervienen dos compositores:

1. Gamescope compone y/o escala el frame del juego.
2. Mutter recibe la superficie resultante y la integra en la composición y presentación del escritorio.

Esto añade:

- otra etapa de composición/presentación;
- sincronización entre Gamescope y el compositor host;
- menos oportunidades de direct scanout/flip;
- trabajo adicional después de que el procesamiento FSR/NIS ya haya terminado.

FSR y NIS siguen teniendo su propio coste de procesamiento, pero en modo nested el frame ya procesado debe atravesar además la ruta del compositor de escritorio.

En UGM Gaming Mode, NIS se expone/desbloquea mediante el plugin de Decky Loader [Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector). El Gamescope incluido con 1.0.0-3 contiene además trabajo de integración específico del proyecto para este flujo. Los cambios exactos del lado de Gamescope ya no están disponibles y por tanto no se describen aquí.

## Qué muestran las mediciones

Los datos son coherentes con ese modelo:

- Wayland linear: ~101–102 FPS
- Wayland FSR/NIS: ~88 FPS
- SDL linear: ~96 FPS
- SDL FSR/NIS: ~88–91 FPS
- DRM linear: ~120 FPS
- DRM FSR/NIS: ~114–117 FPS

FSR y NIS ofrecieron un rendimiento muy similar en estas pruebas. Esto sugiere que una parte importante de la penalización observada en este escenario nested concreto procede de la ruta de presentación/composición y no únicamente del algoritmo de escalado seleccionado.

Es una interpretación respaldada por las mediciones; no demuestra que FSR y NIS tengan un coste interno idéntico.

## Conclusión del proyecto

FSR y NIS no se consideran rotos en Ubuntu Gaming Mode.

La mayor pérdida de rendimiento observada en el escritorio pertenece principalmente a la ruta nested:

```text
Gamescope -> Mutter -> DRM
```

UGM fue diseñado específicamente para proporcionar:

```text
Gamescope -> DRM
```

La implementación actual consigue ese objetivo y reproduce la arquitectura fundamental que permite que Gamescope funcione de forma eficaz como compositor de una sesión dedicada de juego, en lugar de hacerlo como una ventana nested dentro de un escritorio completo.

## Resumen

| Escenario | Linear | FSR / NIS | Penalización relativa FSR/NIS |
|---|---:|---:|---:|
| Nested SDL | ~96 FPS | ~88–91 FPS | ~5–8 % |
| Nested Wayland/Mutter | ~101–102 FPS | ~88 FPS | ~13 % |
| DRM / Gaming Mode | ~120 FPS | ~114–117 FPS | ~2,5–5 % |

Ganancia observada nested → DRM:

- Linear: ~18–25 %
- FSR/NIS: ~25–33 %

## Referencias técnicas

- ValveSoftware/gamescope — README: https://github.com/ValveSoftware/gamescope
- ValveSoftware/gamescope — main/backends: https://github.com/ValveSoftware/gamescope/blob/master/src/main.cpp
- ValveSoftware/gamescope — Wiki: https://github.com/ValveSoftware/gamescope/wiki
