"""Thin unprivileged Decky adapter. Modules are reusable by UGM Options."""
import asyncio
import os
from pathlib import Path
import pwd
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from catalog import Catalog
from shortcuts import saved_shortcuts

VERSION = '1.0.0'


class Plugin:
    def _service(self):
        uid = os.geteuid()
        if uid == 0:
            raise ValueError('El plugin debe ejecutarse como usuario, no como root. Revisa sus flags en Decky.')
        return Catalog(Path(pwd.getpwuid(uid).pw_dir), uid)

    async def _invoke(self, method, *args):
        try:
            result = await asyncio.to_thread(getattr(self._service(), method), *args)
            return dict(ok=True, data=result)
        except (OSError, ValueError, UnicodeError) as error:
            return dict(ok=False, error=str(error))

    async def _main(self):
        pass

    async def _unload(self):
        pass

    async def get_context(self):
        try:
            service = self._service()
            return dict(ok=True, data=dict(version=VERSION, home=str(service.home), locations=service.locations()))
        except (OSError, ValueError, UnicodeError) as error:
            return dict(ok=False, error=str(error))

    async def list_applications(self, query='', page=0):
        return await self._invoke('applications', query, page)

    async def browse(self, path, query='', page=0, hidden=False):
        return await self._invoke('browse', path, query, page, hidden)

    async def inspect_path(self, path):
        return await self._invoke('inspect', path)

    async def make_executable(self, path):
        return await self._invoke('make_executable', path)

    async def validate_draft(self, draft):
        return await self._invoke('validate', draft)

    async def get_saved_shortcuts(self, steam_id):
        try:
            data = await asyncio.to_thread(saved_shortcuts, self._service().home, steam_id)
            return dict(ok=True, data=data)
        except (OSError, ValueError, UnicodeError) as error:
            return dict(ok=False, error=str(error))
