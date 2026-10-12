# VRS y capacidades de pantalla en UGM 2.5.0-5

**Estado:** versión preliminar. La integración y las pruebas
automatizadas están hechas; faltan la comprobación final del botón en
GamepadUI, la prueba A/B en juegos y una prueba física en hardware AMD.

## HDR y VRR

UGM anuncia a Steam la capacidad **general** HDR/VRR del compositor cuando
la preferencia de usuario está en `auto` u `on`; `off` desactiva la ruta.
Gamescope 3.16.30 informa por separado la capacidad del conector activo con
las propiedades Xwayland `GAMESCOPE_DISPLAY_SUPPORTS_HDR` y
`GAMESCOPE_VRR_CAPABLE`, y las actualiza al cambiar de salida. No se fija una
decisión por EDID antes de iniciar Steam. El inventario es diagnóstico y no
sustituye la comprobación del conector activo. El código de GamepadUI es
cerrado: queda por verificar físicamente cómo muestra sus controles con
monitores distintos.

## Selección de VRS durante la instalación

`ugm-vrs-select` exige una relación inequívoca entre DRM y una única GPU
Vulkan hardware identificable, driver/ICD apropiado y VRS por pipeline. Guarda
la ruta en `/etc/ubuntu-gaming-mode/vrs.conf`:

| Ruta | Condición | Instalación |
| --- | --- | --- |
| `radv` | APU integrada RADV con PCI ID conocido en la lista conservadora del selector | Automática; usa el archivo dinámico nativo de Mesa, sin capa UGM. |
| `nvidia` | NVIDIA propietario con features VRS y capa disponible | Pregunta experimental de Debconf, predeterminada en No. |
| `amd` | AMD RADV compatible que no tomó la ruta APU nativa | Pregunta experimental de Debconf, predeterminada en No. |
| `none` | GPU ambigua/no elegible o capa rechazada | No anuncia VRS dinámico. |

La mera presencia de `VK_KHR_fragment_shading_rate` no fuerza VRS. Las capas
personalizadas también comprueban capacidad/driver dentro del proceso. La capa
AMD no está diseñada para AMDVLK ni para el driver Vulkan propietario de AMD.
Si cambia GPU o driver, hay que volver a evaluar la selección mediante
reinstalación/reconfiguración del paquete.

## Componentes y botón de Steam

`packaging/build-deb.sh` compila las capas AMD y NVIDIA de 32 y 64 bits desde
`prototypes/{amd,nvidia}-vrs/` y crea sus manifiestos en
`/usr/lib/ubuntu-gaming-mode/vrs-bundled`. El payload no se registra
globalmente en Vulkan. La ruta aceptada también queda en el estado privado de
instalación (`/var/lib/ubuntu-gaming-mode`, permisos 0700).

Tras arrancar Gamescope y antes de Steam, `post_gamescope_start` prepara la
capa elegida en `~/.local/share/ubuntu-gaming-mode/vrs` desde el **payload
legible** del paquete. Así no intenta leer las copias
del estado privado como usuario normal. La ruta AMD personalizada comparte el
mismo preparador; RADV nativo no necesita ninguna capa. Si preparar la capa
falla, Steam no recibe el anuncio VRS.

Para una ruta funcional, el hook crea un archivo inicial `1x1` en
`~/.local/state/ubuntu-gaming-mode`, exporta `STEAM_USE_DYNAMIC_VRS=1` y
`RADV_FORCE_VRS_CONFIG_FILE`, y para las capas personalizadas exporta también
`UGM_VRS_CONFIG_FILE`, `VK_ADD_LAYER_PATH` y `VK_INSTANCE_LAYERS`. El botón de
Steam debería escribir `1x1`/`2x2` en el archivo. El código cerrado de Steam
impide garantizar su visibilidad sin comprobar GamepadUI en vivo. Los command
buffers ya grabados no cambian hasta que el juego los vuelve a grabar.

La capa NVIDIA superó pruebas de 32/64 bits y renderizado diferencial fuera de
pantalla en RTX 2060. La ruta AMD y RADV pasan pruebas de selección e
integración, **no** validación física AMD. VRS forzado puede afectar calidad;
las capas no prometen ganancia de rendimiento ni compatibilidad con todos los
pipelines/juegos. Los README de cada capa documentan límites y pruebas.

Referencias: [variables RADV VRS de Mesa](https://docs.mesa3d.org/envvars.html#envvar-RADV_FORCE_VRS),
[Khronos `VK_KHR_fragment_shading_rate`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_fragment_shading_rate.html),
[Gamescope HDR/VRR](https://github.com/ValveSoftware/gamescope),
[pruebas D3D12 VRS de vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton/blob/master/tests/d3d12_vrs.c).
