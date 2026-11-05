# Integración opcional de Decky

[English](../../ugm-2.8/decky.md) | **Español** · [Resumen 2.8](README.md)

Desde **2.8.5-1**, el instalador ofrece **Decky Loader** y **UGM Options**: una
entrada con Sharp Filter, Downsampling, Video Output y Non-Steam. Las preguntas
son opcionales y desactivadas por defecto en instalaciones nuevas.

[Funciones, migración y límites](../ugm-2.8.5/README.md) ·
[Fuentes del maestro](../../../integrations/decky/ugm-options/README_ES.md).

Si falta Decky, UGM descarga el instalador CLI **oficial** de SteamDeckHomebrew
fijado en la revisión `9b4e29d2a33b969da11114cce18422a87425f60e`, verifica SHA256
`2403ddbfec8ab320c33729bcb1c8176d74952de71f731ba0c71ded711058be2d`
y lo ejecuta con umask 022. Necesita red, curl y jq; la release de Decky que
descarga depende de upstream. Un fallo opcional avisa sin cancelar UGM. Reintenta
con `sudo dpkg-reconfigure ubuntu-gaming-mode`.

El paquete distribuye solo el maestro, con motores anidados y licencias.
Los ajustes y otros plugins se conservan; una migración fallida restaura carpetas.
No se eliminan ajustes al desinstalar UGM. Las versiones 2.8 anteriores usaban
entradas independientes: consulta su changelog para el historial.
