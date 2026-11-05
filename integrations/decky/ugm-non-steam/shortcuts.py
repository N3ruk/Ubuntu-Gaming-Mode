"""Read-only bounded VDF check, exclusively for the explicitly active account."""
import struct
from pathlib import Path


def read_vdf(data):
    if len(data) > 8 * 1024 * 1024:
        raise ValueError('Biblioteca de accesos demasiado grande para comprobarla.')
    offset, count = 0, 0

    def string():
        nonlocal offset
        end = data.find(b'\0', offset)
        if end < 0 or end - offset > 65536:
            raise ValueError('Cadena VDF dañada.')
        value = data[offset:end].decode('utf-8', errors='strict')
        offset = end + 1
        return value

    def mapping(depth=0):
        nonlocal offset, count
        if depth > 8:
            raise ValueError('VDF demasiado anidado.')
        result = {}
        while offset < len(data):
            kind = data[offset]
            offset += 1
            if kind == 8:
                return result
            count += 1
            if count > 100000:
                raise ValueError('VDF demasiado grande.')
            key = string()
            if kind == 0:
                value = mapping(depth + 1)
            elif kind == 1:
                value = string()
            elif kind == 2 and offset + 4 <= len(data):
                value = struct.unpack_from('<I', data, offset)[0]
                offset += 4
            else:
                raise ValueError('Formato VDF no reconocido.')
            result[key.lower()] = value
        raise ValueError('VDF incompleto.')
    value = mapping()
    if offset != len(data):
        raise ValueError('Datos VDF sobrantes.')
    return value


def saved_shortcuts(home, steam_id):
    if not isinstance(steam_id, str) or not steam_id.isdigit() or len(steam_id) != 17:
        raise ValueError('No se ha identificado la cuenta Steam activa.')
    account = int(steam_id) - 76561197960265728
    if not 0 < account <= 0xffffffff:
        raise ValueError('Cuenta Steam no válida.')
    roots = []
    for root in (Path(home) / '.local/share/Steam', Path(home) / '.steam/steam', Path(home) / '.steam/root'):
        if root.is_dir() and root.resolve() not in roots:
            roots.append(root.resolve())
    matches = [root / 'userdata' / str(account) / 'config/shortcuts.vdf' for root in roots]
    matches = [path for path in matches if path.is_file()]
    if len(matches) > 1:
        raise ValueError('Hay varias bibliotecas Steam para esta cuenta; no se puede asegurar cuál está activa.')
    if not matches:
        return dict(items=[], state='not_saved')
    with matches[0].open('rb') as handle:
        data = handle.read(8 * 1024 * 1024 + 1)
    entries = read_vdf(data).get('shortcuts', {})
    if not isinstance(entries, dict):
        raise ValueError('Biblioteca VDF dañada.')
    result = []
    for item in entries.values():
        if not isinstance(item, dict):
            raise ValueError('Acceso VDF dañado.')
        fields = {key: item.get(key, '') for key in ('appname', 'exe', 'startdir', 'launchoptions')}
        if any(not isinstance(value, str) for value in fields.values()):
            raise ValueError('Campos VDF dañados.')
        appid = item.get('appid', 0)
        if not isinstance(appid, int):
            raise ValueError('Identificador VDF dañado.')
        result.append(dict(appId=appid, name=fields['appname'], exe=fields['exe'],
                           startDir=fields['startdir'], launchOptions=fields['launchoptions']))
    return dict(items=result, state='read')
