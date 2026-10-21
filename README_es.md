# Ubuntu Gaming Mode

[English](README.md) | **Español**

Ubuntu Gaming Mode (UGM) añade a Ubuntu Desktop una sesión dedicada con
Gamescope DRM/KMS y Steam Gamepad UI. Permite alternar entre el escritorio
habitual y Gaming Mode sin sustituir Ubuntu. El paquete estable actual es
**2.8.3-1** para **Ubuntu 26.04 amd64**.

## Por dónde empezar

1. Descarga el `.deb` y `SHA256SUMS` de la
   [última versión](https://github.com/N3ruk/Ubuntu-Gaming-Mode/releases/latest).
2. Sigue la [guía de instalación y verificación](docs/es/instalacion.md).
3. Para funcionamiento, configuración y límites, usa el [índice de documentación](docs/es/README.md).

```bash
# Desde el directorio donde hayas descargado ambos archivos:
sha256sum -c SHA256SUMS
sudo apt install ./ubuntu-gaming-mode_2.8.3-1_amd64.deb
sudo ubuntu-gaming-mode-doctor
```

No utilices un fichero de checksums de otra versión. La instalación o
reconfiguración ofrece funciones opcionales: Decky Loader, ambos plugins,
despertar por mando Bluetooth y las capas VRS experimentales no se activan
todas automáticamente.

## Qué incorpora 2.8

- Runtime dedicado Gamescope 3.16.30 GBM con nitidez selectiva al reducir
  resolución (parche 0010), controlable opcionalmente con un plugin Decky.
- Integración opcional de Decky Loader, Sharp Filter Selector y UGM Downsampling
  Sharpener, con preguntas independientes y una corrección de permisos en 2.8.0-2.
- Opción de despertar por mando Bluetooth, con política USB de denegación por
  defecto **solo en instalaciones nuevas de 2.8 que la activen**. Las
  actualizaciones desde 2.5 conservan su política anterior.
- Capas VRS AMD/NVIDIA de 32/64 bits con selección y fallback conservadores;
  la ruta APU RADV nativa es independiente. El beneficio y la compatibilidad
  dependen de la GPU, el controlador y el juego.
- Tema de iconos UGM-ICONS opcional y desactivado por defecto, instalable y
  activable para el usuario configurado desde la pregunta del instalador.
- Cambio Desktop ↔ Gaming Mode, rollback, preferencias HDR/VRR y Doctor.

Consulta el [resumen de funciones 2.8](docs/es/ugm-2.8/README.md) para conocer
las pruebas y los límites. El [historial](CHANGELOG_es.md) recoge versiones anteriores.

## Mapa del proyecto

| Ruta | Función |
| --- | --- |
| [`docs/`](docs/es/README.md) | Guías, funciones actuales, arquitectura e histórico |
| [`src/`](src/) y [`system/`](system/) | Herramientas de sesión/runtime e integración del sistema |
| [`packaging/`](packaging/) | Metadatos y receta de construcción Debian |
| [`gamescope/`](gamescope/README_es.md) | Serie versionada de parches y procedencia de Gamescope |
| [`integrations/`](integrations/) | Payloads opcionales de Decky y UGM-ICONS |
| [`prototypes/`](prototypes/README_es.md) | Fuentes, pruebas e investigación histórica de componentes experimentales |
| [`tests/`](tests/) | Pruebas automatizadas del proyecto |

Para colaborar: [guía de desarrollo](docs/es/desarrollo.md),
[fuentes y reproducibilidad](SOURCE_es.md) y [avisos de terceros](THIRD_PARTY_NOTICES_es.md).
El código original de UGM usa la [licencia MIT](LICENSE); los componentes de
terceros conservan sus licencias. UGM es independiente de Valve, ChimeraOS,
Bazzite, NVIDIA y Canonical.
