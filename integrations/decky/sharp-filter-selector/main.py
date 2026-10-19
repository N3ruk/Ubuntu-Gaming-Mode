"""Decky backend for selecting Gamescope Sharp filters.

Compatibility policy:

- The validated v0.3.4 FSR/NIS paths remain intact for old Gamescope builds.
- Ubuntu / generic Linux writes Gamescope's modern filter selector and mirrors
  GAMESCOPE_SHARP_FILTER only for compatibility with older builds. It never
  changes the scaler selected by Steam/QAM.
- SteamOS keeps its validated multi-Xwayland NIS path, including the
  LINEAR -> NIS transition and enforcement loop.
- Gamescope builds that advertise SGSR gain a dynamic native-Sharp path:
  Steam's Sharp value 5 is restored for SGSR in SDR and Gamescope performs its
  own FSR fallback for HDR input.

SGSR support is detected from the Gamescope process that owns the active
Xwayland session, never from a possibly unrelated gamescope in PATH.
"""

from __future__ import annotations

import asyncio
import os
import re
import shutil
import subprocess
from pathlib import Path

import decky


DISPLAY_RE = re.compile(r"^:[0-9]+(?:\.[0-9]+)?$")
SHARPNESS_MIN = 0
SHARPNESS_MAX = 5

# SteamOS / upstream Gamescope selectors.
LEGACY_SCALING_LINEAR = 0
LEGACY_SCALING_FSR = 3
LEGACY_SCALING_NIS = 4

NEW_SCALING_LINEAR = 0
NEW_SCALING_NEAREST = 1
NEW_SCALING_FSR = 2
NEW_SCALING_NIS = 3
NEW_SCALING_PIXEL = 4
NEW_SCALING_SGSR = 5

# SteamOS path still performs its validated scaler transitions. Ubuntu's
# filter-only path below deliberately does not write this property.
NEW_SCALER_AUTO = 0
NEW_SCALER_INTEGER = 1
NEW_SCALER_FIT = 2
NEW_SCALER_FILL = 3
NEW_SCALER_STRETCH = 4

# Ubuntu compatibility selector retained for older builds. Gamescope 3.16.30
# consumes GAMESCOPE_NEW_SCALING_FILTER for FSR/NIS/SGSR selection.
UBUNTU_SHARP_FILTER_FSR = 0
UBUNTU_SHARP_FILTER_NIS = 1

FAST_ENFORCE_PASSES = 6
FAST_ENFORCE_INTERVAL = 0.20
STEADY_ENFORCE_INTERVAL = 0.75
UBUNTU_SYNC_INTERVAL = 0.75
TRANSITION_DELAY = 0.08


class Plugin:
    def __init__(self):
        self._nis_requested = False
        self._fsr_override_requested = False
        self._enforce_task: asyncio.Task | None = None
        self._fsr_enforce_task: asyncio.Task | None = None
        self._ubuntu_sync_task: asyncio.Task | None = None
        self._gamescope_probe_cache: dict | None = None

    # ------------------------------------------------------------------
    # PLATFORM
    # ------------------------------------------------------------------

    @staticmethod
    def _is_steamos() -> bool:
        try:
            text = Path("/etc/os-release").read_text(
                encoding="utf-8", errors="ignore"
            ).lower()
        except OSError:
            return False

        markers = (
            "id=steamos",
            'id="steamos"',
            "variant_id=steamdeck",
            'variant_id="steamdeck"',
            "name=steamos",
            'name="steamos',
        )
        return any(marker in text for marker in markers)

    # ------------------------------------------------------------------
    # UBUNTU / GENERIC LINUX
    # Filter selection only. The QAM-owned scaling mode is intentionally left
    # untouched.
    # ------------------------------------------------------------------

    @staticmethod
    def _ubuntu_displays() -> list[tuple[str, int, int, dict[str, str]]]:
        displays: dict[str, tuple[int, int, dict[str, str]]] = {}

        try:
            proc_entries = list(Path("/proc").iterdir())
        except OSError:
            return []

        for proc in proc_entries:
            if not proc.name.isdigit():
                continue

            try:
                stat = proc.stat()
                if stat.st_uid == 0:
                    continue
                environ = (proc / "environ").read_bytes().split(b"\0")
                cmdline = (proc / "cmdline").read_bytes().split(b"\0")
            except (FileNotFoundError, PermissionError, ProcessLookupError, OSError):
                continue

            environment: dict[str, str] = {}
            for entry in environ:
                if b"=" not in entry:
                    continue
                key, value = entry.split(b"=", 1)
                environment[key.decode("ascii", "ignore")] = value.decode(
                    "utf-8", "ignore"
                )

            desktop = environment.get("XDG_CURRENT_DESKTOP", "").lower()
            if "gamescope" not in desktop and not environment.get(
                "GAMESCOPE_WAYLAND_DISPLAY"
            ):
                continue

            display = environment.get("DISPLAY", "")
            if not DISPLAY_RE.fullmatch(display) and cmdline:
                executable = os.path.basename(
                    cmdline[0].decode("utf-8", "ignore")
                ).lower()
                if executable == "xwayland":
                    for argument in cmdline[1:]:
                        candidate = argument.decode("ascii", "ignore")
                        if DISPLAY_RE.fullmatch(candidate):
                            display = candidate
                            environment["DISPLAY"] = display
                            break

            if DISPLAY_RE.fullmatch(display):
                displays[display] = (stat.st_uid, stat.st_gid, environment)

        return [
            (display, *details)
            for display, details in sorted(
                displays.items(),
                key=lambda item: int(item[0][1:].split(".", 1)[0]),
            )
        ]

    @staticmethod
    def _ubuntu_xenv(environment: dict[str, str]) -> dict[str, str]:
        # Preserve the known-working minimal environment exactly.
        xenv = {"DISPLAY": environment.get("DISPLAY", "")}
        if environment.get("XAUTHORITY"):
            xenv["XAUTHORITY"] = environment["XAUTHORITY"]
        return xenv

    @staticmethod
    def _drop_privileges(uid: int, gid: int):
        def drop() -> None:
            if os.geteuid() == 0:
                os.setgroups([])
                os.setgid(gid)
                os.setuid(uid)
        return drop

    @classmethod
    def _ubuntu_write_property(
        cls,
        display: str,
        uid: int,
        gid: int,
        environment: dict[str, str],
        name: str,
        value: int,
    ) -> tuple[bool, str]:
        try:
            completed = subprocess.run(
                [
                    "/usr/bin/xprop",
                    "-display",
                    display,
                    "-root",
                    "-f",
                    name,
                    "32c",
                    "-set",
                    name,
                    str(value),
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=3,
                env=cls._ubuntu_xenv(environment),
                preexec_fn=cls._drop_privileges(uid, gid),
            )
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError) as exc:
            return False, str(exc)

        if completed.returncode != 0:
            return False, (completed.stderr or completed.stdout).strip()
        return True, ""

    @classmethod
    def _ubuntu_read_property(
        cls,
        display: str,
        uid: int,
        gid: int,
        environment: dict[str, str],
        name: str,
    ) -> int | None:
        try:
            completed = subprocess.run(
                ["/usr/bin/xprop", "-display", display, "-root", name],
                check=False,
                capture_output=True,
                text=True,
                timeout=3,
                env=cls._ubuntu_xenv(environment),
                preexec_fn=cls._drop_privileges(uid, gid),
            )
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
            return None

        if completed.returncode != 0:
            return None

        match = re.search(r"=\s*(-?\d+)\s*$", completed.stdout)
        return int(match.group(1)) if match else None

    @classmethod
    def _ubuntu_read_properties(
        cls,
        display: str,
        uid: int,
        gid: int,
        environment: dict[str, str],
        names: tuple[str, ...],
    ) -> dict[str, int | None]:
        values: dict[str, int | None] = {name: None for name in names}
        try:
            completed = subprocess.run(
                ["/usr/bin/xprop", "-display", display, "-root", *names],
                check=False,
                capture_output=True,
                text=True,
                timeout=3,
                env=cls._ubuntu_xenv(environment),
                preexec_fn=cls._drop_privileges(uid, gid),
            )
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
            return values

        for line in completed.stdout.splitlines():
            match = re.match(r"^([^(:\s]+).*?=\s*(-?\d+)\s*$", line)
            if match and match.group(1) in values:
                values[match.group(1)] = int(match.group(2))
        return values

    @classmethod
    def _ubuntu_write_all(
        cls,
        properties: tuple[tuple[str, int], ...],
    ) -> tuple[list[str], list[str]]:
        displays = cls._ubuntu_displays()
        applied: list[str] = []
        errors: list[str] = []

        for display, uid, gid, environment in displays:
            display_errors: list[str] = []
            for name, value in properties:
                ok, error = cls._ubuntu_write_property(
                    display, uid, gid, environment, name, value
                )
                if not ok:
                    display_errors.append(f"{name}: {error}")

            if display_errors:
                errors.append(f"{display}: {'; '.join(display_errors)}")
            else:
                applied.append(display)

        return applied, errors

    async def _ubuntu_restore_native_sharp(self) -> dict:
        capabilities = self._gamescope_capabilities()
        supports_sgsr = bool(capabilities.get("supportsSGSR"))

        if supports_sgsr:
            # Restore Steam's modern Sharp wire value. Gamescope 3.16.29+
            # interprets 5 as SGSR and performs its own HDR-input fallback to FSR.
            # Do not write GAMESCOPE_NEW_SCALING_SCALER: it belongs to QAM.
            properties = (
                ("GAMESCOPE_SHARP_FILTER", UBUNTU_SHARP_FILTER_FSR),
                ("GAMESCOPE_NEW_SCALING_FILTER", NEW_SCALING_SGSR),
            )
        else:
            # v0.3.4 behavior, byte-for-byte equivalent at the property level.
            properties = (("GAMESCOPE_SHARP_FILTER", UBUNTU_SHARP_FILTER_FSR),)

        applied, errors = self._ubuntu_write_all(properties)
        if not applied:
            error = (
                "No accessible Gamescope session was found"
                if not errors
                else "; ".join(errors)
            )
            return {"success": False, "error": error}

        return {
            "success": True,
            "enabled": False,
            "engine": self._native_sharp_engine(),
            "selectedEngine": "sgsr" if supports_sgsr else "fsr",
            "displays": applied,
            "errors": errors,
            "steamos": False,
            "ubuntuCompat": True,
        }

    async def _ubuntu_set_nis_enabled(self, enabled: bool) -> dict:
        if enabled:
            # Gamescope 3.16.30 consumes the modern selector. Mirror the old
            # property for compatibility, but never alter the QAM scaler.
            applied, errors = self._ubuntu_write_all(
                (
                    ("GAMESCOPE_SHARP_FILTER", UBUNTU_SHARP_FILTER_NIS),
                    ("GAMESCOPE_NEW_SCALING_FILTER", NEW_SCALING_NIS),
                )
            )
            if not applied:
                error = (
                    "No accessible Gamescope session was found"
                    if not errors
                    else "; ".join(errors)
                )
                return {"success": False, "error": error}

            return {
                "success": True,
                "enabled": True,
                "engine": "nis",
                "displays": applied,
                "errors": errors,
                "steamos": False,
                "ubuntuCompat": True,
            }

        return await self._ubuntu_restore_native_sharp()

    async def _ubuntu_set_fsr_override(self, enabled: bool) -> dict:
        capabilities = self._gamescope_capabilities()
        if enabled and not capabilities.get("supportsSGSR"):
            return {
                "success": False,
                "error": "FSR override is only needed when the active Gamescope supports SGSR",
            }

        if not enabled:
            return await self._ubuntu_restore_native_sharp()

        # Keep the compatibility property and modern selector aligned on FSR,
        # without overwriting Steam/QAM's scaling mode.
        applied, errors = self._ubuntu_write_all(
            (
                ("GAMESCOPE_SHARP_FILTER", UBUNTU_SHARP_FILTER_FSR),
                ("GAMESCOPE_NEW_SCALING_FILTER", NEW_SCALING_FSR),
            )
        )

        if not applied:
            error = (
                "No accessible Gamescope session was found"
                if not errors
                else "; ".join(errors)
            )
            return {"success": False, "error": error}

        return {
            "success": True,
            "enabled": True,
            "engine": "fsr",
            "selectedEngine": "fsr",
            "displays": applied,
            "errors": errors,
            "steamos": False,
            "ubuntuCompat": True,
        }

    async def _ubuntu_set_sharpness(self, level: int) -> dict:
        raw_value = 20 - (level * 4)
        applied, errors = self._ubuntu_write_all(
            (
                ("GAMESCOPE_SHARPNESS", raw_value),
                ("GAMESCOPE_FSR_SHARPNESS", raw_value),
            )
        )

        if not applied:
            error = (
                "No accessible Gamescope session was found"
                if not errors
                else "; ".join(errors)
            )
            return {"success": False, "error": error}

        return {
            "success": True,
            "level": level,
            "rawValue": raw_value,
            "displays": applied,
            "errors": errors,
        }

    async def _ubuntu_get_status(self) -> dict:
        displays = self._ubuntu_displays()
        if not displays:
            return {
                "success": False,
                "error": "No accessible Gamescope session was found",
                "steamos": False,
                "ubuntuCompat": True,
            }

        nis_enabled = False
        raw_sharpness: int | None = None
        available: list[str] = []
        diagnostics: list[dict] = []

        for display, uid, gid, environment in displays:
            available.append(display)

            engine_value = self._ubuntu_read_property(
                display,
                uid,
                gid,
                environment,
                "GAMESCOPE_SHARP_FILTER",
            )
            modern = self._ubuntu_read_property(
                display, uid, gid, environment, "GAMESCOPE_NEW_SCALING_FILTER"
            )
            if modern == NEW_SCALING_NIS or (
                modern is None and engine_value == UBUNTU_SHARP_FILTER_NIS
            ):
                nis_enabled = True

            for property_name in (
                "GAMESCOPE_SHARPNESS",
                "GAMESCOPE_FSR_SHARPNESS",
            ):
                value = self._ubuntu_read_property(
                    display, uid, gid, environment, property_name
                )
                if value is not None:
                    raw_sharpness = max(0, min(20, value))
                    break

            diagnostics.append(
                {
                    "display": display,
                    "compatFilter": engine_value,
                    "newFilter": modern,
                    "newScaler": self._ubuntu_read_property(
                        display,
                        uid,
                        gid,
                        environment,
                        "GAMESCOPE_NEW_SCALING_SCALER",
                    ),
                }
            )

        level = (
            round((20 - raw_sharpness) / 4)
            if raw_sharpness is not None
            else None
        )

        if self._nis_requested or nis_enabled:
            engine = "nis"
        elif self._fsr_override_requested:
            engine = "fsr"
        else:
            engine = self._native_sharp_engine()

        return {
            "success": True,
            "enabled": engine == "nis",
            "requested": self._nis_requested,
            "fsrOverride": self._fsr_override_requested,
            "engine": engine,
            "level": level,
            "rawValue": raw_sharpness,
            "displays": available,
            "diagnostics": diagnostics,
            "steamos": False,
            "ubuntuCompat": True,
            "enforcing": bool(
                self._ubuntu_sync_task and not self._ubuntu_sync_task.done()
            ),
        }

    async def _ubuntu_sync_qam_loop(self) -> None:
        """Keep QAM state coherent across Gamescope's Xwayland roots.

        Steam writes its QAM properties to Xwayland server 0. Gamescope treats
        property events from every root as global compositor state, so stale
        values on another root can later win. Server 0 is therefore the sole
        authority here. The loop only mirrors values that QAM already chose.

        Gamescope 3.16.29+ exposes SGSR as filter 5 while Steam's Sharp wire
        value remains FSR (2). In native mode only, translate that one value to
        SGSR. Explicit plugin overrides retain NIS (3) or FSR (2).
        """
        try:
            while not self._is_steamos():
                await asyncio.sleep(UBUNTU_SYNC_INTERVAL)

                displays = self._ubuntu_displays()
                if not displays:
                    continue

                property_names = (
                    "GAMESCOPE_XWAYLAND_SERVER_ID",
                    "GAMESCOPE_NEW_SCALING_FILTER",
                    "GAMESCOPE_NEW_SCALING_SCALER",
                )
                states = {
                    target[0]: self._ubuntu_read_properties(*target, property_names)
                    for target in displays
                }
                controller = next(
                    (
                        target
                        for target in displays
                        if states[target[0]]["GAMESCOPE_XWAYLAND_SERVER_ID"] == 0
                    ),
                    displays[0],
                )
                display = controller[0]
                qam_filter = states[display]["GAMESCOPE_NEW_SCALING_FILTER"]
                qam_scaler = states[display]["GAMESCOPE_NEW_SCALING_SCALER"]

                desired_filter = qam_filter
                if self._nis_requested:
                    desired_filter = NEW_SCALING_NIS
                elif self._fsr_override_requested:
                    desired_filter = NEW_SCALING_FSR
                elif (
                    qam_filter == NEW_SCALING_FSR
                    and self._gamescope_capabilities().get("supportsSGSR")
                ):
                    desired_filter = NEW_SCALING_SGSR

                valid_filter = desired_filter in {
                    NEW_SCALING_LINEAR,
                    NEW_SCALING_NEAREST,
                    NEW_SCALING_FSR,
                    NEW_SCALING_NIS,
                    NEW_SCALING_PIXEL,
                    NEW_SCALING_SGSR,
                }
                valid_scaler = qam_scaler in {
                    NEW_SCALER_AUTO,
                    NEW_SCALER_INTEGER,
                    NEW_SCALER_FIT,
                    NEW_SCALER_FILL,
                    NEW_SCALER_STRETCH,
                }

                changes: list[str] = []
                for target_display, target_uid, target_gid, target_environment in displays:
                    if valid_filter:
                        current_filter = states[target_display][
                            "GAMESCOPE_NEW_SCALING_FILTER"
                        ]
                        if current_filter != desired_filter:
                            ok, error = self._ubuntu_write_property(
                                target_display,
                                target_uid,
                                target_gid,
                                target_environment,
                                "GAMESCOPE_NEW_SCALING_FILTER",
                                desired_filter,
                            )
                            if ok:
                                changes.append(
                                    f"{target_display}:filter={desired_filter}"
                                )
                            else:
                                decky.logger.warning(
                                    "Sharp Filter Selector: could not sync filter on %s: %s",
                                    target_display,
                                    error,
                                )

                    if valid_scaler:
                        current_scaler = states[target_display][
                            "GAMESCOPE_NEW_SCALING_SCALER"
                        ]
                        if current_scaler != qam_scaler:
                            ok, error = self._ubuntu_write_property(
                                target_display,
                                target_uid,
                                target_gid,
                                target_environment,
                                "GAMESCOPE_NEW_SCALING_SCALER",
                                qam_scaler,
                            )
                            if ok:
                                changes.append(
                                    f"{target_display}:scaler={qam_scaler}"
                                )
                            else:
                                decky.logger.warning(
                                    "Sharp Filter Selector: could not sync scaler on %s: %s",
                                    target_display,
                                    error,
                                )

                if changes:
                    decky.logger.info(
                        "Sharp Filter Selector: synchronized QAM state from %s (%s)",
                        display,
                        ", ".join(changes),
                    )

        except asyncio.CancelledError:
            raise
        except Exception:
            decky.logger.exception(
                "Sharp Filter Selector: Ubuntu QAM synchronization loop failed"
            )

    def _start_ubuntu_sync(self) -> None:
        if self._is_steamos():
            return

        task = self._ubuntu_sync_task
        if task and not task.done():
            task.cancel()
        self._ubuntu_sync_task = asyncio.create_task(self._ubuntu_sync_qam_loop())

    async def _stop_ubuntu_sync(self) -> None:
        task = self._ubuntu_sync_task
        self._ubuntu_sync_task = None
        if not task:
            return
        if not task.done():
            task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass

    # ------------------------------------------------------------------
    # STEAMOS
    # Separate experimental path; no code below is used on Ubuntu.
    # ------------------------------------------------------------------

    @staticmethod
    def _xprop_binary() -> str | None:
        for candidate in (
            shutil.which("xprop"),
            "/usr/bin/xprop",
            "/usr/local/bin/xprop",
            "/bin/xprop",
        ):
            if candidate and os.path.isfile(candidate) and os.access(candidate, os.X_OK):
                return candidate
        return None

    @staticmethod
    def _read_proc_environment(proc: Path) -> dict[str, str]:
        environment: dict[str, str] = {}
        try:
            entries = (proc / "environ").read_bytes().split(b"\0")
        except (FileNotFoundError, PermissionError, ProcessLookupError, OSError):
            return environment

        for entry in entries:
            if b"=" not in entry:
                continue
            key, value = entry.split(b"=", 1)
            environment[key.decode("ascii", "ignore")] = value.decode(
                "utf-8", "ignore"
            )
        return environment

    @staticmethod
    def _ppid(proc: Path) -> int | None:
        try:
            for line in (proc / "status").read_text(
                encoding="utf-8", errors="ignore"
            ).splitlines():
                if line.startswith("PPid:"):
                    return int(line.split(":", 1)[1].strip())
        except (
            FileNotFoundError,
            PermissionError,
            ProcessLookupError,
            OSError,
            ValueError,
        ):
            pass
        return None

    @staticmethod
    def _proc_name(proc: Path) -> str:
        try:
            return (proc / "comm").read_text(
                encoding="utf-8", errors="ignore"
            ).strip()
        except (FileNotFoundError, PermissionError, ProcessLookupError, OSError):
            return ""

    @staticmethod
    def _proc_cmdline(proc: Path) -> list[str]:
        try:
            return [
                part.decode("utf-8", "ignore")
                for part in (proc / "cmdline").read_bytes().split(b"\0")
                if part
            ]
        except (FileNotFoundError, PermissionError, ProcessLookupError, OSError):
            return []

    @classmethod
    def _looks_like_gamescope_process(cls, proc: Path) -> bool:
        name = cls._proc_name(proc).lower()
        cmdline = cls._proc_cmdline(proc)
        argv0 = Path(cmdline[0]).name.lower() if cmdline else ""

        candidates = {name, argv0}
        for candidate in candidates:
            if not candidate:
                continue
            if candidate == "gamescope":
                return True
            if candidate.startswith("gamescope-") and not candidate.startswith(
                ("gamescopectl", "gamescope-session")
            ):
                return True
        return False

    @classmethod
    def _active_gamescope_candidates(cls) -> list[tuple[int, int, int, dict[str, str], int]]:
        """Return likely Gamescope processes, highest-confidence first.

        The strongest signal is a Gamescope ancestor of an active Xwayland.
        Direct Gamescope processes with Gamescope session markers are retained as
        a fallback for layouts where Xwayland is not a direct child.
        """
        try:
            proc_entries = [p for p in Path("/proc").iterdir() if p.name.isdigit()]
        except OSError:
            return []

        scored: dict[int, tuple[int, int, dict[str, str], int]] = {}

        def remember(proc: Path, score: int) -> None:
            try:
                stat = proc.stat()
            except (FileNotFoundError, PermissionError, ProcessLookupError, OSError):
                return
            pid = int(proc.name)
            environment = cls._read_proc_environment(proc)
            previous = scored.get(pid)
            if previous is None or score > previous[3]:
                scored[pid] = (stat.st_uid, stat.st_gid, environment, score)

        # Prefer the Gamescope process that actually owns an Xwayland used by the session.
        for proc in proc_entries:
            name = cls._proc_name(proc).lower()
            cmdline = cls._proc_cmdline(proc)
            argv0 = Path(cmdline[0]).name.lower() if cmdline else ""
            if "xwayland" not in name and "xwayland" not in argv0:
                continue

            parent = cls._ppid(proc)
            depth = 0
            seen: set[int] = set()
            while parent and parent not in seen and depth < 12:
                seen.add(parent)
                parent_proc = Path("/proc") / str(parent)
                if cls._looks_like_gamescope_process(parent_proc):
                    remember(parent_proc, 200 - depth)
                    break
                parent = cls._ppid(parent_proc)
                depth += 1

        # Fallback: direct Gamescope processes, weighted by session markers.
        for proc in proc_entries:
            if not cls._looks_like_gamescope_process(proc):
                continue
            environment = cls._read_proc_environment(proc)
            score = 20
            if environment.get("GAMESCOPE_WAYLAND_DISPLAY"):
                score += 40
            if "gamescope" in environment.get("XDG_CURRENT_DESKTOP", "").lower():
                score += 20
            remember(proc, score)

        return [
            (pid, uid, gid, environment, score)
            for pid, (uid, gid, environment, score) in sorted(
                scored.items(), key=lambda item: (-item[1][3], item[0])
            )
        ]

    @classmethod
    def _probe_gamescope_process(
        cls,
        pid: int,
        uid: int,
        gid: int,
        environment: dict[str, str],
    ) -> dict:
        proc_exe = f"/proc/{pid}/exe"
        try:
            executable = os.readlink(proc_exe)
        except OSError:
            executable = proc_exe

        probe_env = os.environ.copy()
        for key in ("PATH", "HOME", "XDG_RUNTIME_DIR", "LD_LIBRARY_PATH"):
            value = environment.get(key)
            if value:
                probe_env[key] = value
        # Avoid injecting game/overlay libraries into the short-lived probe.
        probe_env.pop("LD_PRELOAD", None)

        help_text = ""
        version_text = ""
        errors: list[str] = []

        for arg, field in (("--help", "help"), ("--version", "version")):
            try:
                completed = subprocess.run(
                    [proc_exe, arg],
                    check=False,
                    capture_output=True,
                    text=True,
                    timeout=3,
                    env=probe_env,
                    preexec_fn=cls._drop_privileges(uid, gid),
                )
                output = (completed.stdout or "") + "\n" + (completed.stderr or "")
                if field == "help":
                    help_text = output
                else:
                    version_text = output
            except (FileNotFoundError, PermissionError, subprocess.TimeoutExpired, OSError) as exc:
                errors.append(f"{arg}: {exc}")

        help_lower = help_text.lower()
        supports_sgsr = bool(re.search(r"(?:^|[\s,(])sgsr(?:[\s,)]|$)", help_lower))
        evidence = "help" if supports_sgsr else ""

        # Read-only binary fallback for custom builds whose --help probe cannot run.
        if not supports_sgsr:
            try:
                saw_sgsr = False
                saw_snapdragon = False
                tail = b""
                with open(proc_exe, "rb", buffering=0) as binary:
                    while True:
                        chunk = binary.read(1024 * 1024)
                        if not chunk:
                            break
                        data = (tail + chunk).lower()
                        saw_sgsr = saw_sgsr or b"sgsr" in data
                        saw_snapdragon = saw_snapdragon or b"snapdragon" in data
                        if saw_sgsr and saw_snapdragon:
                            break
                        tail = data[-32:]
                if saw_sgsr and saw_snapdragon:
                    supports_sgsr = True
                    evidence = "binary"
            except (OSError, PermissionError):
                pass

        version_match = re.search(
            r"(?:gamescope(?:\s+version)?\s+)?(\d+\.\d+(?:\.\d+)?)",
            version_text,
            re.IGNORECASE,
        )
        version = version_match.group(1) if version_match else None

        return {
            "pid": pid,
            "executable": executable,
            "version": version,
            "supportsSGSR": supports_sgsr,
            "evidence": evidence or "none",
            "errors": errors,
        }

    def _gamescope_capabilities(self) -> dict:
        candidates = self._active_gamescope_candidates()
        if not candidates:
            self._gamescope_probe_cache = None
            return {
                "pid": None,
                "executable": None,
                "version": None,
                "supportsSGSR": False,
                "evidence": "none",
                "errors": ["No active Gamescope process found"],
            }

        pid, uid, gid, environment, _score = candidates[0]
        cached = self._gamescope_probe_cache
        if cached and cached.get("pid") == pid:
            return cached

        probed = self._probe_gamescope_process(pid, uid, gid, environment)
        self._gamescope_probe_cache = probed
        return probed

    def _session_property_values(self, name: str) -> list[int]:
        values: list[int] = []
        if self._is_steamos():
            for display, uid, gid, environment, _source in self._steamos_targets():
                value = self._steamos_read_property(
                    display, uid, gid, environment, name
                )
                if value is not None:
                    values.append(value)
        else:
            for display, uid, gid, environment in self._ubuntu_displays():
                value = self._ubuntu_read_property(
                    display, uid, gid, environment, name
                )
                if value is not None:
                    values.append(value)
        return values

    def _hdr_state(self) -> dict:
        app_values = self._session_property_values(
            "GAMESCOPE_COLOR_APP_WANTS_HDR_FEEDBACK"
        )
        output_values = self._session_property_values(
            "GAMESCOPE_DISPLAY_HDR_ENABLED"
        )

        # Only unambiguous application feedback can hide the FSR override.
        # Missing/invalid/conflicting feedback is unknown, not display HDR.
        app_known = bool(app_values) and all(
            value in (0, 1) for value in app_values
        ) and len(set(app_values)) == 1
        app_hdr = app_known and app_values[0] == 1
        output_hdr = any(value == 1 for value in output_values)

        # Output state is diagnostic only. SDR games can run on HDR output.
        # Read fresh feedback every time; do not retain HDR across game changes.
        hdr_input = app_hdr
        return {
            "hdrInput": hdr_input,
            "hdrInputKnown": app_known,
            "hdrOutputEnabled": output_hdr,
            "hdrSource": "app" if app_known else "unknown",
        }

    def _native_sharp_engine(self) -> str:
        capabilities = self._gamescope_capabilities()
        if not capabilities.get("supportsSGSR"):
            return "fsr"
        return "fsr" if self._hdr_state()["hdrInput"] else "sgsr"

    @classmethod
    def _steamos_env_displays(
        cls,
    ) -> list[tuple[str, int, int, dict[str, str], str]]:
        displays: dict[str, tuple[int, int, dict[str, str], str]] = {}

        try:
            proc_entries = list(Path("/proc").iterdir())
        except OSError:
            return []

        for proc in proc_entries:
            if not proc.name.isdigit():
                continue

            try:
                stat = proc.stat()
                if stat.st_uid == 0:
                    continue
            except (FileNotFoundError, PermissionError, ProcessLookupError, OSError):
                continue

            environment = cls._read_proc_environment(proc)
            desktop = environment.get("XDG_CURRENT_DESKTOP", "").lower()
            if "gamescope" not in desktop and not environment.get(
                "GAMESCOPE_WAYLAND_DISPLAY"
            ):
                continue

            display = environment.get("DISPLAY", "")
            if DISPLAY_RE.fullmatch(display):
                displays[display] = (
                    stat.st_uid,
                    stat.st_gid,
                    environment,
                    "environment",
                )

        return [
            (display, *details)
            for display, details in sorted(
                displays.items(),
                key=lambda item: int(item[0][1:].split(".", 1)[0]),
            )
        ]

    @classmethod
    def _steamos_xwaylands(
        cls,
    ) -> list[tuple[str, int, int, dict[str, str], str]]:
        displays: dict[str, tuple[int, int, dict[str, str], str]] = {}

        try:
            proc_entries = list(Path("/proc").iterdir())
        except OSError:
            return []

        for proc in proc_entries:
            if not proc.name.isdigit():
                continue

            name = cls._proc_name(proc).lower()
            cmdline = cls._proc_cmdline(proc)
            argv0 = Path(cmdline[0]).name.lower() if cmdline else ""

            if "xwayland" not in name and "xwayland" not in argv0:
                continue

            try:
                stat = proc.stat()
                if stat.st_uid == 0:
                    continue
            except (FileNotFoundError, PermissionError, ProcessLookupError, OSError):
                continue

            display = next(
                (arg for arg in cmdline[1:] if DISPLAY_RE.fullmatch(arg)),
                "",
            )
            if not display:
                continue

            environment = cls._read_proc_environment(proc)

            parent_is_gamescope = False
            parent = cls._ppid(proc)
            if parent:
                parent_proc = Path("/proc") / str(parent)
                parent_name = cls._proc_name(parent_proc).lower()
                parent_cmd = " ".join(cls._proc_cmdline(parent_proc)).lower()
                parent_is_gamescope = (
                    "gamescope" in parent_name or "gamescope" in parent_cmd
                )

            wayland_marker = " ".join(
                (
                    environment.get("WAYLAND_DISPLAY", ""),
                    environment.get("GAMESCOPE_WAYLAND_DISPLAY", ""),
                )
            ).lower()

            if not parent_is_gamescope and "gamescope" not in wayland_marker:
                continue

            if "-auth" in cmdline:
                try:
                    auth = cmdline[cmdline.index("-auth") + 1]
                    if auth:
                        environment["XAUTHORITY"] = auth
                except (ValueError, IndexError):
                    pass

            environment["DISPLAY"] = display
            displays[display] = (
                stat.st_uid,
                stat.st_gid,
                environment,
                "xwayland",
            )

        return [
            (display, *details)
            for display, details in sorted(
                displays.items(),
                key=lambda item: int(item[0][1:].split(".", 1)[0]),
            )
        ]

    @classmethod
    def _steamos_targets(
        cls,
    ) -> list[tuple[str, int, int, dict[str, str], str]]:
        merged: dict[str, tuple[int, int, dict[str, str], str]] = {}

        for display, uid, gid, environment, source in cls._steamos_env_displays():
            merged[display] = (uid, gid, environment, source)

        for display, uid, gid, environment, source in cls._steamos_xwaylands():
            merged[display] = (uid, gid, environment, source)

        return [
            (display, *details)
            for display, details in sorted(
                merged.items(),
                key=lambda item: int(item[0][1:].split(".", 1)[0]),
            )
        ]

    @staticmethod
    def _steamos_xenv(environment: dict[str, str]) -> dict[str, str]:
        xenv = os.environ.copy()
        xenv["DISPLAY"] = environment.get("DISPLAY", "")
        if environment.get("XAUTHORITY"):
            xenv["XAUTHORITY"] = environment["XAUTHORITY"]
        else:
            xenv.pop("XAUTHORITY", None)
        return xenv

    @classmethod
    def _steamos_write_property(
        cls,
        display: str,
        uid: int,
        gid: int,
        environment: dict[str, str],
        name: str,
        value: int,
    ) -> tuple[bool, str]:
        xprop = cls._xprop_binary()
        if not xprop:
            return False, "xprop is not installed or could not be found in PATH"

        try:
            completed = subprocess.run(
                [
                    xprop,
                    "-display",
                    display,
                    "-root",
                    "-f",
                    name,
                    "32c",
                    "-set",
                    name,
                    str(value),
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=3,
                env=cls._steamos_xenv(environment),
                preexec_fn=cls._drop_privileges(uid, gid),
            )
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError) as exc:
            return False, str(exc)

        if completed.returncode != 0:
            return False, (completed.stderr or completed.stdout).strip()
        return True, ""

    @classmethod
    def _steamos_read_property(
        cls,
        display: str,
        uid: int,
        gid: int,
        environment: dict[str, str],
        name: str,
    ) -> int | None:
        xprop = cls._xprop_binary()
        if not xprop:
            return None

        try:
            completed = subprocess.run(
                [xprop, "-display", display, "-root", name],
                check=False,
                capture_output=True,
                text=True,
                timeout=3,
                env=cls._steamos_xenv(environment),
                preexec_fn=cls._drop_privileges(uid, gid),
            )
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
            return None

        if completed.returncode != 0:
            return None

        match = re.search(r"=\s*(-?\d+)\s*$", completed.stdout)
        return int(match.group(1)) if match else None

    @classmethod
    def _steamos_write_to_targets(
        cls,
        targets: list[tuple[str, int, int, dict[str, str], str]],
        properties: tuple[tuple[str, int], ...],
    ) -> tuple[list[str], list[str]]:
        applied: list[str] = []
        errors: list[str] = []

        for display, uid, gid, environment, _source in targets:
            display_errors: list[str] = []
            for name, value in properties:
                ok, error = cls._steamos_write_property(
                    display, uid, gid, environment, name, value
                )
                if not ok:
                    display_errors.append(f"{name}: {error}")

            if display_errors:
                errors.append(f"{display}: {'; '.join(display_errors)}")
            else:
                applied.append(display)

        return applied, errors

    @staticmethod
    def _steamos_properties(enabled: bool) -> tuple[tuple[str, int], ...]:
        # This is the validated v0.3.4 SteamOS FSR/NIS property set.
        if enabled:
            return (
                ("GAMESCOPE_NEW_SCALING_SCALER", NEW_SCALER_AUTO),
                ("GAMESCOPE_NEW_SCALING_FILTER", NEW_SCALING_NIS),
                ("GAMESCOPE_SCALING_FILTER", LEGACY_SCALING_NIS),
                ("GAMESCOPE_SHARP_FILTER", UBUNTU_SHARP_FILTER_NIS),
            )

        return (
            ("GAMESCOPE_NEW_SCALING_SCALER", NEW_SCALER_AUTO),
            ("GAMESCOPE_NEW_SCALING_FILTER", NEW_SCALING_FSR),
            ("GAMESCOPE_SCALING_FILTER", LEGACY_SCALING_FSR),
            ("GAMESCOPE_SHARP_FILTER", UBUNTU_SHARP_FILTER_FSR),
        )

    def _steamos_native_properties(self) -> tuple[tuple[str, int], ...]:
        if self._gamescope_capabilities().get("supportsSGSR"):
            # Steam's modern Sharp wire value. Do not send legacy value 5: the
            # legacy selector only defines values 0..4.
            return (
                ("GAMESCOPE_NEW_SCALING_SCALER", NEW_SCALER_AUTO),
                ("GAMESCOPE_NEW_SCALING_FILTER", NEW_SCALING_SGSR),
            )
        return self._steamos_properties(False)

    async def _steamos_restore_native_sharp(self) -> tuple[list[str], list[str]]:
        return self._steamos_write_to_targets(
            self._steamos_targets(),
            self._steamos_native_properties(),
        )

    async def _force_steamos_nis_transition(
        self,
    ) -> tuple[list[str], list[str]]:
        targets = self._steamos_targets()
        if not targets:
            return [], []

        self._steamos_write_to_targets(
            targets,
            (
                ("GAMESCOPE_NEW_SCALING_SCALER", NEW_SCALER_AUTO),
                ("GAMESCOPE_NEW_SCALING_FILTER", NEW_SCALING_LINEAR),
                ("GAMESCOPE_SCALING_FILTER", LEGACY_SCALING_LINEAR),
            ),
        )

        await asyncio.sleep(TRANSITION_DELAY)

        return self._steamos_write_to_targets(
            targets,
            self._steamos_properties(True),
        )

    @classmethod
    def _steamos_state_on_target(
        cls,
        display: str,
        uid: int,
        gid: int,
        environment: dict[str, str],
        source: str,
    ) -> dict:
        compat = cls._steamos_read_property(
            display, uid, gid, environment, "GAMESCOPE_SHARP_FILTER"
        )
        legacy = cls._steamos_read_property(
            display, uid, gid, environment, "GAMESCOPE_SCALING_FILTER"
        )
        modern = cls._steamos_read_property(
            display, uid, gid, environment, "GAMESCOPE_NEW_SCALING_FILTER"
        )
        scaler = cls._steamos_read_property(
            display, uid, gid, environment, "GAMESCOPE_NEW_SCALING_SCALER"
        )
        fsr_feedback = cls._steamos_read_property(
            display, uid, gid, environment, "GAMESCOPE_FSR_FEEDBACK"
        )
        server_id = cls._steamos_read_property(
            display, uid, gid, environment, "GAMESCOPE_XWAYLAND_SERVER_ID"
        )

        if modern is not None:
            display_nis = modern == NEW_SCALING_NIS
            display_fsr = modern == NEW_SCALING_FSR
            display_sgsr = modern == NEW_SCALING_SGSR
        elif legacy is not None:
            display_nis = legacy == LEGACY_SCALING_NIS
            display_fsr = legacy == LEGACY_SCALING_FSR
            display_sgsr = False
        else:
            display_nis = compat == UBUNTU_SHARP_FILTER_NIS
            display_fsr = compat == UBUNTU_SHARP_FILTER_FSR
            display_sgsr = False

        return {
            "display": display,
            "source": source,
            "serverId": server_id,
            "legacy": legacy,
            "modern": modern,
            "compat": compat,
            "scaler": scaler,
            "fsrFeedback": fsr_feedback,
            "displayNis": display_nis,
            "displayFsr": display_fsr,
            "displaySgsr": display_sgsr,
        }

    def _start_enforcer(self) -> None:
        if not self._is_steamos():
            return

        task = self._enforce_task
        if task and not task.done():
            task.cancel()

        self._enforce_task = asyncio.create_task(self._enforce_nis_loop())

    async def _stop_enforcer(self) -> None:
        task = self._enforce_task
        self._enforce_task = None

        if not task:
            return

        if not task.done():
            task.cancel()

        try:
            await task
        except asyncio.CancelledError:
            pass

    async def _enforce_nis_loop(self) -> None:
        fast_passes = FAST_ENFORCE_PASSES

        try:
            while self._nis_requested and self._is_steamos():
                delay = (
                    FAST_ENFORCE_INTERVAL
                    if fast_passes > 0
                    else STEADY_ENFORCE_INTERVAL
                )
                await asyncio.sleep(delay)

                if not self._nis_requested:
                    break

                targets = self._steamos_targets()
                needs_reapply = fast_passes > 0

                for target in targets:
                    state = self._steamos_state_on_target(*target)
                    if not state["displayNis"] or state["fsrFeedback"] == 1:
                        needs_reapply = True
                        break

                if needs_reapply and targets:
                    self._steamos_write_to_targets(
                        targets,
                        self._steamos_properties(True),
                    )

                if fast_passes > 0:
                    fast_passes -= 1

        except asyncio.CancelledError:
            raise
        except Exception:
            decky.logger.exception(
                "Sharp Filter Selector: SteamOS NIS enforcement loop failed"
            )

    def _start_fsr_enforcer(self) -> None:
        if not self._is_steamos():
            return

        task = self._fsr_enforce_task
        if task and not task.done():
            task.cancel()

        self._fsr_enforce_task = asyncio.create_task(self._enforce_fsr_loop())

    async def _stop_fsr_enforcer(self) -> None:
        task = self._fsr_enforce_task
        self._fsr_enforce_task = None

        if not task:
            return

        if not task.done():
            task.cancel()

        try:
            await task
        except asyncio.CancelledError:
            pass

    async def _enforce_fsr_loop(self) -> None:
        fast_passes = FAST_ENFORCE_PASSES

        try:
            while self._fsr_override_requested and self._is_steamos():
                delay = (
                    FAST_ENFORCE_INTERVAL
                    if fast_passes > 0
                    else STEADY_ENFORCE_INTERVAL
                )
                await asyncio.sleep(delay)

                if not self._fsr_override_requested:
                    break

                targets = self._steamos_targets()
                needs_reapply = fast_passes > 0

                for target in targets:
                    state = self._steamos_state_on_target(*target)
                    if not state["displayFsr"]:
                        needs_reapply = True
                        break

                if needs_reapply and targets:
                    self._steamos_write_to_targets(
                        targets,
                        self._steamos_properties(False),
                    )

                if fast_passes > 0:
                    fast_passes -= 1

        except asyncio.CancelledError:
            raise
        except Exception:
            decky.logger.exception(
                "Sharp Filter Selector: SteamOS FSR enforcement loop failed"
            )

    async def _steamos_set_nis_enabled(self, enabled: bool) -> dict:
        self._nis_requested = enabled

        if enabled:
            self._fsr_override_requested = False
            await self._stop_fsr_enforcer()
            applied, errors = await self._force_steamos_nis_transition()
            self._start_enforcer()
        else:
            await self._stop_enforcer()
            if self._fsr_override_requested:
                applied, errors = self._steamos_write_to_targets(
                    self._steamos_targets(),
                    self._steamos_properties(False),
                )
            else:
                applied, errors = await self._steamos_restore_native_sharp()

        if not applied:
            error = (
                "No accessible Gamescope/Xwayland target was found"
                if not errors
                else "; ".join(errors)
            )
            return {
                "success": False,
                "error": error,
                "enabled": enabled,
                "engine": "nis" if enabled else self._native_sharp_engine(),
                "steamos": True,
            }

        return {
            "success": True,
            "enabled": enabled,
            "engine": "nis" if enabled else (
                "fsr" if self._fsr_override_requested else self._native_sharp_engine()
            ),
            "selectedEngine": "nis" if enabled else (
                "fsr" if self._fsr_override_requested else (
                    "sgsr" if self._gamescope_capabilities().get("supportsSGSR") else "fsr"
                )
            ),
            "displays": applied,
            "errors": errors,
            "steamos": True,
            "enforcing": bool(
                self._enforce_task and not self._enforce_task.done()
            ),
        }

    async def _steamos_set_fsr_override(self, enabled: bool) -> dict:
        capabilities = self._gamescope_capabilities()
        if enabled and not capabilities.get("supportsSGSR"):
            return {
                "success": False,
                "error": "FSR override is only needed when the active Gamescope supports SGSR",
            }

        self._fsr_override_requested = enabled

        if enabled:
            self._nis_requested = False
            await self._stop_enforcer()
            applied, errors = self._steamos_write_to_targets(
                self._steamos_targets(),
                self._steamos_properties(False),
            )
            self._start_fsr_enforcer()
        else:
            await self._stop_fsr_enforcer()
            if self._nis_requested:
                applied, errors = self._steamos_write_to_targets(
                    self._steamos_targets(),
                    self._steamos_properties(True),
                )
            else:
                applied, errors = await self._steamos_restore_native_sharp()

        if not applied:
            error = (
                "No accessible Gamescope/Xwayland target was found"
                if not errors
                else "; ".join(errors)
            )
            return {"success": False, "error": error}

        return {
            "success": True,
            "enabled": enabled,
            "engine": "fsr" if enabled else self._native_sharp_engine(),
            "selectedEngine": "fsr" if enabled else (
                "sgsr" if capabilities.get("supportsSGSR") else "fsr"
            ),
            "displays": applied,
            "errors": errors,
            "steamos": True,
            "enforcing": bool(
                self._fsr_enforce_task and not self._fsr_enforce_task.done()
            ),
        }

    async def _steamos_set_sharpness(self, level: int) -> dict:
        raw_value = 20 - (level * 4)
        applied, errors = self._steamos_write_to_targets(
            self._steamos_targets(),
            (
                ("GAMESCOPE_SHARPNESS", raw_value),
                ("GAMESCOPE_FSR_SHARPNESS", raw_value),
            ),
        )

        if not applied:
            error = (
                "No accessible Gamescope/Xwayland target was found"
                if not errors
                else "; ".join(errors)
            )
            return {"success": False, "error": error}

        return {
            "success": True,
            "level": level,
            "rawValue": raw_value,
            "displays": applied,
            "errors": errors,
        }

    async def _steamos_get_status(self) -> dict:
        targets = self._steamos_targets()
        if not targets:
            return {
                "success": False,
                "error": "No accessible Gamescope/Xwayland target was found",
                "requested": self._nis_requested,
                "steamos": True,
            }

        nis_enabled = False
        raw_sharpness: int | None = None
        available: list[str] = []
        diagnostics: list[dict] = []

        for target in targets:
            display, uid, gid, environment, source = target
            state = self._steamos_state_on_target(*target)
            available.append(display)

            display_nis = state["displayNis"]
            if state["fsrFeedback"] == 1:
                display_nis = False
            nis_enabled = nis_enabled or display_nis

            for property_name in (
                "GAMESCOPE_SHARPNESS",
                "GAMESCOPE_FSR_SHARPNESS",
            ):
                value = self._steamos_read_property(
                    display, uid, gid, environment, property_name
                )
                if value is not None:
                    raw_sharpness = max(0, min(20, value))
                    break

            diagnostics.append(
                {
                    "display": display,
                    "source": source,
                    "serverId": state["serverId"],
                    "legacyFilter": state["legacy"],
                    "newFilter": state["modern"],
                    "compatFilter": state["compat"],
                    "newScaler": state["scaler"],
                    "fsrFeedback": state["fsrFeedback"],
                }
            )

        level = (
            round((20 - raw_sharpness) / 4)
            if raw_sharpness is not None
            else None
        )

        if self._nis_requested or nis_enabled:
            engine = "nis"
        elif self._fsr_override_requested:
            engine = "fsr"
        else:
            engine = self._native_sharp_engine()

        return {
            "success": True,
            "enabled": engine == "nis",
            "requested": self._nis_requested,
            "fsrOverride": self._fsr_override_requested,
            "engine": engine,
            "level": level,
            "rawValue": raw_sharpness,
            "displays": available,
            "diagnostics": diagnostics,
            "xprop": self._xprop_binary(),
            "steamos": True,
            "enforcing": bool(
                (self._enforce_task and not self._enforce_task.done())
                or (self._fsr_enforce_task and not self._fsr_enforce_task.done())
            ),
        }

    # ------------------------------------------------------------------
    # DECKY API
    # ------------------------------------------------------------------

    async def set_nis_enabled(self, enabled: bool) -> dict:
        if not isinstance(enabled, bool):
            return {"success": False, "error": "Invalid NIS state"}

        if enabled:
            self._fsr_override_requested = False
            self._nis_requested = True
        else:
            self._nis_requested = False

        if self._is_steamos():
            return await self._steamos_set_nis_enabled(enabled)

        if not enabled and self._fsr_override_requested:
            return await self._ubuntu_set_fsr_override(True)
        return await self._ubuntu_set_nis_enabled(enabled)

    async def set_fsr_override(self, enabled: bool) -> dict:
        if not isinstance(enabled, bool):
            return {"success": False, "error": "Invalid FSR override state"}

        if enabled:
            self._nis_requested = False
            self._fsr_override_requested = True
        else:
            self._fsr_override_requested = False

        if self._is_steamos():
            return await self._steamos_set_fsr_override(enabled)

        return await self._ubuntu_set_fsr_override(enabled)

    async def get_capabilities(self) -> dict:
        probe = self._gamescope_capabilities()
        hdr = self._hdr_state()
        supports_sgsr = bool(probe.get("supportsSGSR"))
        native_engine = "fsr" if (not supports_sgsr or hdr["hdrInput"]) else "sgsr"

        return {
            "success": probe.get("pid") is not None,
            "gamescopePid": probe.get("pid"),
            "gamescopeExecutable": probe.get("executable"),
            "gamescopeVersion": probe.get("version"),
            "supportsSGSR": supports_sgsr,
            "sgsrEvidence": probe.get("evidence", "none"),
            "hdrInput": hdr["hdrInput"],
            "hdrInputKnown": hdr["hdrInputKnown"],
            "hdrOutputEnabled": hdr["hdrOutputEnabled"],
            "hdrSource": hdr["hdrSource"],
            "nativeSharpEngine": native_engine,
            "showFsrOverride": supports_sgsr and not hdr["hdrInput"],
            "steamos": self._is_steamos(),
            "errors": probe.get("errors", []),
        }

    async def set_nis_sharpness(self, level: int) -> dict:
        if (
            isinstance(level, bool)
            or not isinstance(level, int)
            or not SHARPNESS_MIN <= level <= SHARPNESS_MAX
        ):
            return {
                "success": False,
                "error": "Sharpness must be between 0 and 5",
            }

        if self._is_steamos():
            return await self._steamos_set_sharpness(level)

        return await self._ubuntu_set_sharpness(level)

    async def get_status(self) -> dict:
        if self._is_steamos():
            return await self._steamos_get_status()

        return await self._ubuntu_get_status()

    async def _main(self):
        self._start_ubuntu_sync()
        decky.logger.info(
            "Sharp Filter Selector loaded (SteamOS=%s, Ubuntu compatibility path=%s)",
            self._is_steamos(),
            not self._is_steamos(),
        )

    async def _unload(self):
        self._nis_requested = False
        self._fsr_override_requested = False
        await self._stop_ubuntu_sync()
        await self._stop_enforcer()
        await self._stop_fsr_enforcer()
        decky.logger.info("Sharp Filter Selector unloaded")
