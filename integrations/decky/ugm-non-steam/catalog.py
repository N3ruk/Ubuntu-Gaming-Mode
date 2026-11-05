"""Unprivileged discovery. Adapted from Add Non-Steam Games (BSD-3-Clause).
No shell commands, installers or Steam writes. See NOTICE.md for provenance.
"""
import configparser
import os
from pathlib import Path
import re
import shutil
import stat

PATH = '/usr/local/bin:/usr/bin:/bin:/snap/bin'
NON_PROGRAMS = {'.dll', '.so', '.zip', '.7z', '.rar', '.tar', '.gz', '.iso',
                '.txt', '.md', '.png', '.jpg', '.jpeg', '.json', '.ini', '.cfg',
                '.msi', '.bat', '.cmd', '.desktop', '.lnk', '.pdf'}


def checked_text(value, limit=4096):
    if not isinstance(value, str) or len(value) > limit or any(ord(c) < 32 for c in value):
        raise ValueError('Texto no válido o demasiado largo.')
    return value


def path_value(value):
    checked_text(value)
    if not value.startswith('/') or '"' in value:
        raise ValueError('Selecciona una ruta absoluta sin comillas dobles.')
    return Path(value)


def quote_arg(value):
    return '"' + ''.join('\\' + c if c in '\\"$`' else c for c in value) + '"'


def desktop_unescape(value):
    return re.sub(r'\\([sntr\\])', lambda m: {'s': ' ', 'n': '\n', 't': '\t',
                                            'r': '\r', '\\': '\\'}[m[1]], value)


def exec_tokens(value):
    """Freedesktop Exec: double quotes/escapes, never shell expansion."""
    value = desktop_unescape(value)
    result, token, quoted, started = [], [], False, False
    i = 0
    while i < len(value):
        c = value[i]
        if c == '\\':
            i += 1
            if i >= len(value):
                raise ValueError('Escape Exec incompleto.')
            if quoted and value[i] not in '\\"$`':
                token.append('\\')
            token.append(value[i])
            started = True
        elif c == '"':
            quoted = not quoted
            started = True
        elif c.isspace() and not quoted:
            if started:
                result.append(''.join(token))
                token, started = [], False
        else:
            if not quoted and c in "'<>~|&;$`()*?#":
                raise ValueError('Sintaxis Exec no admitida.')
            token.append(c)
            started = True
        i += 1
    if quoted:
        raise ValueError('Comillas Exec incompletas.')
    if started:
        result.append(''.join(token))
    return result


def expand_fields(tokens, name, desktop, icon):
    result = []
    for token in tokens:
        if token in ('@@', '@@u', '%f', '%F', '%u', '%U'):
            continue
        if token == '%i':
            if icon:
                result.extend(['--icon', icon])
            continue
        def replace(match):
            code = match[1]
            if code == '%':
                return '%'
            if code == 'c':
                return name
            if code == 'k':
                return str(desktop)
            raise ValueError('Código Exec no admitido.')
        result.append(re.sub(r'%(.)', replace, token))
        if token.endswith('%') and not token.endswith('%%'):
            raise ValueError('Código Exec incompleto.')
    return result


def localized_name(entry, language):
    language = language.split('.', 1)[0]
    base, _, modifier = language.partition('@')
    lang = base.split('_')[0]
    choices = [language, base, lang + '@' + modifier if modifier else lang, lang]
    for code in dict.fromkeys(choices):
        if entry.get('Name[' + code + ']'):
            return desktop_unescape(entry['Name[' + code + ']'])
    return desktop_unescape(entry.get('Name', ''))


def application_roots(home, env):
    data = Path(env.get('XDG_DATA_HOME') or home / '.local/share')
    roots = [data / 'applications', home / '.local/share/flatpak/exports/share/applications',
             Path('/var/lib/flatpak/exports/share/applications'),
             Path('/var/lib/snapd/desktop/applications')]
    roots += [Path(d) / 'applications' for d in env.get('XDG_DATA_DIRS',
               '/usr/local/share:/usr/share').split(':') if d.startswith('/')]
    return list(dict.fromkeys(p for p in roots if p.is_absolute()))


class Catalog:
    def __init__(self, home, uid, env=None):
        self.home, self.uid = Path(home), uid
        self.env = dict(os.environ if env is None else env)
        self.search_path = str(self.home / '.local/bin') + ':' + PATH

    def binary(self, token):
        path = Path(token) if token.startswith('/') else Path(shutil.which(token, path=self.search_path) or '/__missing__')
        return str(path) if path.is_file() and os.access(path, os.X_OK) else None

    def application(self, desktop):
        try:
            if desktop.stat().st_size > 128 * 1024:
                return None
            parser = configparser.ConfigParser(interpolation=None, strict=False)
            parser.optionxform = str
            parser.read_string(desktop.read_text(encoding='utf-8'))
            entry = parser['Desktop Entry']
            yes = lambda key: entry.get(key, '').lower() == 'true'
            if entry.get('Type', '') != 'Application' or yes('Hidden') or yes('NoDisplay') or yes('Terminal'):
                return None
            desktops = set(self.env.get('XDG_CURRENT_DESKTOP', '').split(':')) - {''}
            if entry.get('OnlyShowIn') and not desktops.intersection(entry['OnlyShowIn'].split(';')):
                return None
            if desktops.intersection(entry.get('NotShowIn', '').split(';')):
                return None
            if entry.get('TryExec') and not self.binary(desktop_unescape(entry['TryExec'])):
                return None
            language = self.env.get('LC_ALL') or self.env.get('LC_MESSAGES') or self.env.get('LANG') or 'en'
            name = checked_text(localized_name(entry, language), 256).strip()
            if not name:
                return None
            tokens = expand_fields(exec_tokens(entry.get('Exec', '')), name, desktop, entry.get('Icon', ''))
            if not tokens or '%' in tokens[0]:
                return None
            exe = self.binary(tokens[0])
            if not exe:
                return None
            path_value(exe)
            start_dir = desktop_unescape(entry.get('Path', '')) or str(self.home)
            if not path_value(start_dir).is_dir():
                return None
            args = checked_text(' '.join(quote_arg(checked_text(t)) for t in tokens[1:]))
            source = 'Flatpak' if 'flatpak' in str(desktop) or Path(exe).name == 'flatpak' else 'Snap' if '/snap' in str(desktop) or Path(exe).name == 'snap' else 'Linux'
            return dict(id=str(desktop), name=name, exe=exe, startDir=start_dir,
                        launchOptions=args, source=source, windows=False, executable=True, compatTool='')
        except (OSError, ValueError, KeyError, configparser.Error, UnicodeError):
            return None

    def applications(self, query='', page=0):
        checked_text(query, 256)
        if not isinstance(page, int) or page < 0 or page > 1000:
            raise ValueError('Página no válida.')
        found, seen, scanned = [], set(), 0
        for root in application_roots(self.home, self.env):
            for directory, dirs, files in os.walk(root, followlinks=False):
                dirs.sort()
                for filename in sorted(files):
                    if not filename.endswith('.desktop'):
                        continue
                    scanned += 1
                    if scanned > 20000:
                        raise ValueError('Demasiadas entradas de aplicaciones; limita los directorios XDG.')
                    desktop = Path(directory) / filename
                    key = desktop.relative_to(root).as_posix().replace('/', '-')
                    if key in seen:
                        continue
                    seen.add(key)  # Hidden entries must mask lower-priority copies too.
                    app = self.application(desktop)
                    if app and query.casefold() in (app['name'] + ' ' + app['source']).casefold():
                        found.append(app)
        found.sort(key=lambda app: (app['name'].casefold(), app['id']))
        return dict(items=found[page * 40:(page + 1) * 40], total=len(found), page=page)

    def noexec(self, path):
        return bool(os.statvfs(path).f_flag & getattr(os, 'ST_NOEXEC', 8))

    def inspect(self, raw):
        path = path_value(raw)
        if not path.is_file() or not os.access(path, os.R_OK):
            raise ValueError('El archivo no existe o no se puede leer.')
        suffix = path.suffix.lower()
        if suffix in NON_PROGRAMS:
            raise ValueError('Selecciona un programa Linux, AppImage o un ejecutable .exe; no un archivo de datos o instalador MSI.')
        windows = suffix == '.exe'
        with path.open('rb') as handle:
            signature = handle.read(4)
        if windows and not signature.startswith(b'MZ'):
            raise ValueError('El archivo no tiene cabecera de ejecutable Windows.')
        if not windows and not (signature.startswith(b'\x7fELF') or signature.startswith(b'#!')):
            raise ValueError('No se reconoce un binario Linux ni un script con intérprete.')
        if self.noexec(path):
            raise ValueError('La unidad está montada con noexec. Mueve el programa a una unidad que permita ejecución.')
        return dict(name=path.stem, exe=str(path), startDir=str(path.parent), launchOptions='',
                    source='Archivo Windows' if windows else 'Archivo Linux', windows=windows,
                    executable=windows or os.access(path, os.X_OK), compatTool='')

    def make_executable(self, raw):
        path = path_value(raw)
        info = self.inspect(raw)
        if info['windows']:
            raise ValueError('Los ejecutables Windows no requieren este cambio.')
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        try:
            st = os.fstat(fd)
            if st.st_uid != self.uid or not stat.S_ISREG(st.st_mode):
                raise ValueError('Solo puedes habilitar un archivo regular de tu propiedad.')
            os.fchmod(fd, stat.S_IMODE(st.st_mode) | stat.S_IXUSR)
        finally:
            os.close(fd)
        return self.inspect(raw)

    def locations(self):
        places = [dict(name='Carpeta personal', path=str(self.home))]
        dirs = self.home / '.config/user-dirs.dirs'
        if dirs.is_file() and dirs.stat().st_size < 16384:
            for line in dirs.read_text().splitlines():
                match = re.fullmatch(r'XDG_(DOWNLOAD|DESKTOP|DOCUMENTS)_DIR="([^"]+)"', line.strip())
                if match:
                    raw = match[2].replace('$HOME', str(self.home))
                    if '$' not in raw and Path(raw).is_dir():
                        places.append(dict(name={'DOWNLOAD': 'Descargas', 'DESKTOP': 'Escritorio', 'DOCUMENTS': 'Documentos'}[match[1]], path=raw))
        for root in (Path('/media') / self.home.name, Path('/run/media') / self.home.name, Path('/mnt')):
            if root.is_dir():
                places.append(dict(name='Unidades · ' + root.name, path=str(root)))
        return places

    def browse(self, raw, query='', page=0, hidden=False):
        path = path_value(raw)
        checked_text(query, 256)
        if not isinstance(page, int) or page < 0 or page > 10000:
            raise ValueError('Página no válida.')
        items = []
        with os.scandir(path) as entries:
            for index, entry in enumerate(entries):
                if index >= 30000:
                    raise ValueError('Carpeta demasiado grande. Selecciona una subcarpeta.')
                if not hidden and entry.name.startswith('.'):
                    continue
                if query.casefold() not in entry.name.casefold():
                    continue
                try:
                    folder = entry.is_dir()
                    if not folder and (not entry.is_file() or Path(entry.name).suffix.lower() in NON_PROGRAMS):
                        continue
                    items.append(dict(name=entry.name, path=entry.path, folder=folder))
                except OSError:
                    continue
        items.sort(key=lambda item: (not item['folder'], item['name'].casefold()))
        return dict(path=str(path), parent=str(path.parent), items=items[page * 40:(page + 1) * 40],
                    total=len(items), page=page)

    def validate(self, draft):
        if not isinstance(draft, dict):
            raise ValueError('Acceso no válido.')
        name = checked_text(draft.get('name'), 256).strip()
        if not name:
            raise ValueError('Escribe un nombre.')
        exe = path_value(draft.get('exe'))
        info = self.inspect(str(exe))
        if not info['executable']:
            raise ValueError('El destino no tiene permiso de ejecución.')
        start = path_value(draft.get('startDir'))
        if not start.is_dir() or not os.access(start, os.X_OK):
            raise ValueError('La carpeta de inicio no está disponible.')
        tool = checked_text(draft.get('compatTool', ''), 256)
        if info['windows'] and not tool:
            raise ValueError('Selecciona una herramienta Proton para el ejecutable Windows.')
        return dict(name=name, exe=str(exe), startDir=str(start),
                    launchOptions=checked_text(draft.get('launchOptions', '')), compatTool=tool, windows=info['windows'])
