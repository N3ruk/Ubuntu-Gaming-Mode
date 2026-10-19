# Avisos de terceros

[English](THIRD_PARTY_NOTICES.md) | **Español**

Ubuntu Gaming Mode contiene o deriva de software de terceros. La licencia MIT
de la raíz del repositorio se aplica únicamente al código original del proyecto
UGM. El software de terceros conserva sus respectivos derechos de autor y
condiciones de licencia.

## ChimeraOS gamescope-session

Parte del runtime de sesión de UGM está basado en o deriva de
[ChimeraOS/gamescope-session](https://github.com/ChimeraOS/gamescope-session).

Licencia upstream: MIT.

Copyright © 2020-2023 Joaquín Ignacio Aramendía

El texto legal completo de la licencia upstream se conserva sin traducir en:

- [licenses/CHIMERAOS-GAMESCOPE-SESSION-LICENSE](licenses/CHIMERAOS-GAMESCOPE-SESSION-LICENSE)

## Valve Gamescope

UGM 2.0.0-1, 2.5.0-5 y 2.8.0-2 distribuyen un binario modificado de Gamescope basado en
[ValveSoftware/gamescope](https://github.com/ValveSoftware/gamescope).

El proyecto principal Gamescope utiliza BSD 2-Clause y su archivo LICENSE
upstream también contiene avisos correspondientes a componentes incluidos o
derivados, entre ellos código de LUT 3D tetraédrica de OpenColorIO y el
compilador de efectos de ReShade.

Copyright (c) 2013-2022, Valve Corporation  
Copyright (c) 2022, NVIDIA CORPORATION  
All rights reserved.

El texto legal upstream completo y sus avisos se conservan sin traducir en:

- [licenses/GAMESCOPE-LICENSE](licenses/GAMESCOPE-LICENSE)

El binario Gamescope incluido con UGM es una build custom documentada por versión.
Su base upstream, serie completa de patches, árbol final y registro de build se
publican en `gamescope/versions/3.16.30-gbm/`; consulta
[gamescope/README_es.md](gamescope/README_es.md).

## Sharp Filter Selector

UGM 2.8.0-2 incluye como opción el plugin
[Sharp Filter Selector](https://github.com/N3ruk/Sharp-Filter-Selector),
con licencia BSD-3-Clause conservada en
[integrations/decky/sharp-filter-selector/LICENSE](integrations/decky/sharp-filter-selector/LICENSE),
y UGM Downsampling Sharpener. Los plugins se instalan solo si se seleccionan
sus opciones; Decky Loader se descarga por separado desde el instalador oficial.

## Marcas y nombres de proyectos

Steam, SteamOS, Steam Deck y Gamescope están asociados con Valve Corporation.
NVIDIA y GeForce están asociados con NVIDIA Corporation. Ubuntu está asociado
con Canonical Ltd.

Ubuntu Gaming Mode es un proyecto comunitario independiente y no está afiliado
ni respaldado por Valve, NVIDIA, Canonical, Bazzite o ChimeraOS.

> Este documento en español es un resumen informativo. Los textos legales
> originales de las licencias se conservan sin traducir en los archivos
> enlazados.

## Tema de iconos UGM-ICONS

El tema opcional UGM-ICONS procede del paquete proporcionado por N3ruk para
esta versión. Su archivo `COPYING` declara GPL-3.0; se conserva junto con
`AUTHORS` en el paquete y dentro del archivo del tema. Los iconos derivados de
terceros mantienen los derechos que les correspondan.
