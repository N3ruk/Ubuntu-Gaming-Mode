"""Decky controller for the UGM-only Gamescope downsampling filter.

No Steam scaling property is read or written. The plugin only controls its own
UGM_* root properties in the matching Gamescope Xwayland sessions.
"""
from __future__ import annotations

import asyncio
import json
import os
import re
import subprocess
import threading
import time
from pathlib import Path

import decky

DISPLAY_RE = re.compile(r"^:[0-9]+(?:\.[0-9]+)?$")
PROPERTIES = (
    "UGM_DOWNSAMPLE_SUPPORTED",
    "UGM_DOWNSAMPLE_DETECTED",
    "UGM_DOWNSAMPLE_ACTIVE",
    "UGM_DOWNSAMPLE_ENABLED",
    "UGM_DOWNSAMPLE_STRENGTH",
)


class Plugin:
    def __init__(self):
        self.enabled = True
        self.strength = 25
        self._task = None
        self._settings = None
        self._lock = threading.RLock()
        self._display_cache = {}
        self._display_cache_at = float("-inf")

    @staticmethod
    def _displays():
        found = {}
        for proc in Path("/proc").iterdir():
            if not proc.name.isdigit():
                continue
            try:
                stat = proc.stat()
                if stat.st_uid == 0:
                    continue
                cmd = (proc / "cmdline").read_bytes().split(b"\0")
                env_bytes = (proc / "environ").read_bytes().split(b"\0")
            except (OSError, PermissionError):
                continue
            env = {}
            for item in env_bytes:
                if b"=" in item:
                    key, value = item.split(b"=", 1)
                    env[key.decode("ascii", "ignore")] = value.decode("utf-8", "ignore")
            if "gamescope" not in env.get("XDG_CURRENT_DESKTOP", "").lower() and not env.get("GAMESCOPE_WAYLAND_DISPLAY"):
                continue
            display = env.get("DISPLAY", "")
            if not DISPLAY_RE.fullmatch(display) and cmd and Path(cmd[0].decode("utf-8", "ignore")).name.lower() == "xwayland":
                display = next((arg.decode("ascii", "ignore") for arg in cmd[1:] if DISPLAY_RE.fullmatch(arg.decode("ascii", "ignore"))), "")
            if DISPLAY_RE.fullmatch(display):
                xenv = {"DISPLAY": display}
                if env.get("XAUTHORITY"):
                    xenv["XAUTHORITY"] = env["XAUTHORITY"]
                found[display] = (stat.st_uid, stat.st_gid, xenv)
        return found

    @classmethod
    def _xprop(cls, display, credentials, *args):
        uid, gid, xenv = credentials
        try:
            # Popen's native credential handling is safe from worker threads;
            # Python preexec_fn is not (it may deadlock after fork).
            credentials_args = {"user": uid, "group": gid, "extra_groups": []} if os.geteuid() == 0 else {}
            return subprocess.run(
                ["/usr/bin/xprop", "-display", display, "-root", *args],
                capture_output=True, text=True, timeout=3, env=xenv,
                check=False, **credentials_args,
            )
        except (OSError, subprocess.TimeoutExpired):
            return None

    @classmethod
    def _read(cls, display, credentials):
        result = cls._xprop(display, credentials, *PROPERTIES)
        values = {name: None for name in PROPERTIES}
        if result is None:
            return values
        for line in result.stdout.splitlines():
            match = re.match(r"^([^(:\s]+).*?=\s*(-?\d+)\s*$", line)
            if match and match.group(1) in values:
                values[match.group(1)] = int(match.group(2))
        return values

    @classmethod
    def _write(cls, display, credentials, name, value):
        result = cls._xprop(display, credentials, "-f", name, "32c", "-set", name, str(value))
        return result is not None and result.returncode == 0

    def _save(self):
        self._settings.parent.mkdir(parents=True, exist_ok=True)
        tmp = self._settings.with_suffix(".tmp")
        tmp.write_text(json.dumps({"enabled": self.enabled, "strength": self.strength}), encoding="utf-8")
        tmp.replace(self._settings)

    def _status(self, synchronize=False):
        with self._lock:
            return self._status_locked(synchronize)

    def _status_locked(self, synchronize=False):
        # UI polling and background sync share discovery. Scan /proc at most
        # once per ten seconds, not once for every request / slider movement.
        now = time.monotonic()
        if now - self._display_cache_at >= 10:
            self._display_cache = self._displays()
            self._display_cache_at = now
        sessions = []
        for display, credentials in self._display_cache.items():
            values = self._read(display, credentials)
            if values["UGM_DOWNSAMPLE_SUPPORTED"] != 1:
                continue
            if synchronize:
                changed = False
                if values["UGM_DOWNSAMPLE_ENABLED"] != int(self.enabled):
                    changed = True
                    self._write(display, credentials, "UGM_DOWNSAMPLE_ENABLED", int(self.enabled))
                if values["UGM_DOWNSAMPLE_STRENGTH"] != self.strength:
                    changed = True
                    self._write(display, credentials, "UGM_DOWNSAMPLE_STRENGTH", self.strength)
                if changed:
                    values = self._read(display, credentials)
            sessions.append({
                "display": display,
                "detected": values["UGM_DOWNSAMPLE_DETECTED"] == 1,
                "active": values["UGM_DOWNSAMPLE_ACTIVE"] == 1,
                "enabledApplied": values["UGM_DOWNSAMPLE_ENABLED"] == int(self.enabled),
                "strengthApplied": values["UGM_DOWNSAMPLE_STRENGTH"] == self.strength,
            })
        return {"enabled": self.enabled, "strength": self.strength, "sessions": sessions}

    async def get_status(self):
        return await asyncio.to_thread(self._status)

    def _set_preference(self, name, value):
        with self._lock:
            setattr(self, name, value)
            self._save()
            return self._status_locked(True)

    async def set_enabled(self, enabled: bool):
        return await asyncio.to_thread(self._set_preference, "enabled", bool(enabled))

    async def set_strength(self, strength: int):
        if not isinstance(strength, int) or isinstance(strength, bool):
            raise ValueError("strength must be an integer")
        return await asyncio.to_thread(self._set_preference, "strength", max(0, min(100, strength)))

    async def _sync_loop(self):
        while True:
            try:
                await asyncio.to_thread(self._status, True)
            except Exception as exc:
                decky.logger.warning("UGM downsampling sync: %s", exc)
            await asyncio.sleep(3)

    async def _main(self):
        self._settings = Path(decky.DECKY_PLUGIN_SETTINGS_DIR) / "ugm-downsampling-sharpener.json"
        try:
            saved = json.loads(self._settings.read_text(encoding="utf-8"))
            self.enabled = bool(saved.get("enabled", True))
            self.strength = max(0, min(100, int(saved.get("strength", 25))))
        except (OSError, ValueError, TypeError):
            pass
        self._task = asyncio.create_task(self._sync_loop())

    async def _unload(self):
        if self._task:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass
        # A standalone/unloaded plugin leaves the filter off.
        self.enabled = False
        await asyncio.to_thread(self._status, True)
