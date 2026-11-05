"""Decky entry point. Only the broker and Video Output retain root."""
from pathlib import Path
import sys
import decky

sys.path.insert(0, str(Path(__file__).resolve().parent))
from backend import Broker


class Plugin:
    async def _main(self):
        self.broker = Broker(Path(__file__).resolve().parent,
                             Path(decky.DECKY_PLUGIN_SETTINGS_DIR).resolve().parent,
                             decky.logger)
        await self.broker.start()

    async def module_status(self):
        return self.broker.status()

    async def module_call(self, module, method, args):
        return await self.broker.call(module, method, args)

    async def _unload(self):
        if hasattr(self, 'broker'):
            await self.broker.close()
