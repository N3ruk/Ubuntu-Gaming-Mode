"""Read-only UGM session discovery; never modify the compositor or its wrapper."""
import os
from pathlib import Path

PROC = Path('/proc')
RUNTIME = '/usr/lib/ubuntu-gaming-mode/gamescope'
WRAPPER = '/usr/lib/ubuntu-gaming-mode/gamescope-session-plus-runtime'


def process_context(entry):
    """Accept only UGM's real DRM compositor, not nested/desktop Gamescope."""
    try:
        if os.readlink(entry / 'exe') != RUNTIME:
            return None
        args = (entry / 'cmdline').read_bytes().split(b'\0')
        backend = args.index(b'--backend')
        if args[backend + 1] != b'drm' or not any(a in args for a in (b'-e', b'--steam')):
            return None
        stat = (entry / 'stat').read_text().split(') ', 1)[1].split()
        uid = entry.stat().st_uid
        if uid <= 0:
            return None
        return {'pid': int(entry.name), 'start': stat[19], 'uid': uid}
    except (OSError, ValueError, IndexError):
        return None


def gaming_context():
    for entry in PROC.iterdir():
        if entry.name.isdigit():
            context = process_context(entry)
            if context:
                return context
    return None


def session_matches(request):
    current = process_context(PROC / str(request['pid']))
    return bool(current and current['start'] == request['start'] and current['uid'] == request['uid'])


def session_environment(request):
    """Read DISPLAY/socket from this verified UGM runtime's descendants only.

    SteamOS writes gamescope-environment; UGM instead starts Steam from its
    wrapper with the environment received on Gamescope's readiness socket.
    """
    if not session_matches(request):
        raise RuntimeError('UGM Gaming session ended or was replaced')
    compositor = PROC / str(request['pid'])
    parent = int((compositor / 'stat').read_text().split(') ', 1)[1].split()[1])
    wrapper = PROC / str(parent)
    if WRAPPER.encode() not in (wrapper / 'cmdline').read_bytes().split(b'\0'):
        raise RuntimeError('Unrecognized UGM session wrapper')
    if wrapper.stat().st_uid != request['uid']:
        raise RuntimeError('UGM session owner changed')

    def belongs(entry):
        seen = set()
        for _ in range(32):
            pid = int(entry.name)
            if pid == parent:
                return True
            if pid <= 1 or pid in seen:
                return False
            seen.add(pid)
            stat = (entry / 'stat').read_text().split(') ', 1)[1].split()
            entry = PROC / stat[1]
        return False

    candidates = []
    for entry in PROC.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            if entry.stat().st_uid != request['uid'] or not belongs(entry):
                continue
            data = {}
            for item in (entry / 'environ').read_bytes().split(b'\0'):
                key, sep, value = item.partition(b'=')
                if sep and key in (b'DISPLAY', b'GAMESCOPE_WAYLAND_DISPLAY', b'XAUTHORITY'):
                    data[key.decode()] = value.decode()
            if data.get('DISPLAY') and data.get('GAMESCOPE_WAYLAND_DISPLAY'):
                if data not in candidates:
                    candidates.append(data)
        except (OSError, ValueError, IndexError):
            continue
    if not candidates:
        raise RuntimeError('UGM display environment is not ready')
    return candidates
