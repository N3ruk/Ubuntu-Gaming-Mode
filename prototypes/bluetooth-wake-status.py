#!/usr/bin/env python3
"""Inventario de solo lectura para el prototipo de wake por Bluetooth."""

import json
import sys
from pathlib import Path

import dbus


def nearest_wakeup(adapter):
    node = Path("/sys/class/bluetooth", adapter)
    if not node.exists():
        return None
    resolved = node.resolve()
    for ancestor in (resolved, *resolved.parents):
        wakeup = ancestor / "power" / "wakeup"
        if wakeup.is_file():
            try:
                return {"path": str(wakeup), "state": wakeup.read_text().strip()}
            except OSError as exc:
                return {"path": str(wakeup), "error": str(exc)}
        if ancestor == Path("/sys"):
            break
    return None


def inventory():
    bus = dbus.SystemBus()
    manager = dbus.Interface(bus.get_object("org.bluez", "/"),
                             "org.freedesktop.DBus.ObjectManager")
    objects = manager.GetManagedObjects()
    adapters = {}
    devices = []
    for path, interfaces in objects.items():
        if "org.bluez.Adapter1" in interfaces:
            name = str(path).rsplit("/", 1)[-1]
            adapters[name] = {"adapter": name, "wakeup": nearest_wakeup(name)}
        if "org.bluez.Device1" in interfaces:
            data = interfaces["org.bluez.Device1"]
            if not bool(data.get("Paired", False)):
                continue
            adapter = str(data.get("Adapter", "")).rsplit("/", 1)[-1]
            devices.append({
                "adapter": adapter,
                "address": str(data.get("Address", "")),
                "alias": str(data.get("Alias", data.get("Name", ""))),
                "icon": str(data.get("Icon", "")),
                "wake_allowed": bool(data["WakeAllowed"]) if "WakeAllowed" in data else None,
            })
    return {"adapters": sorted(adapters.values(), key=lambda x: x["adapter"]),
            "paired_devices": sorted(devices, key=lambda x: (x["adapter"], x["address"]))}


if __name__ == "__main__":
    try:
        print(json.dumps(inventory(), ensure_ascii=False, indent=2))
    except dbus.DBusException as exc:
        print(f"No se pudo consultar BlueZ: {exc}", file=sys.stderr)
        sys.exit(1)
