#!/usr/bin/env python3
"""Read-only Bluetooth suspend-wake candidate probe; never proves a real wake."""

import json
from pathlib import Path
import dbus


def read(path):
    try:
        return path.read_text().strip()
    except OSError:
        return None


def probe_adapter(name):
    node = (Path("/sys/class/bluetooth") / name / "device").resolve()
    usb = next((p for p in (node, *node.parents)
                if (p / "idVendor").is_file() and (p / "idProduct").is_file()), None)
    result = {"adapter": name, "transport": "usb" if usb else "other"}
    if usb is None:
        result.update(candidate=False, reason="no-usb-wake-detection")
        return result
    attributes = read(usb / "bmAttributes")
    try:
        remote_wake = bool(int(attributes, 16) & 0x20)
    except (TypeError, ValueError):
        remote_wake = None
    wakeup = read(usb / "power/wakeup")
    parents = []
    for parent in usb.parents:
        if parent == Path("/sys") or not str(parent).startswith("/sys/devices"):
            break
        state = read(parent / "power/wakeup")
        if state is not None:
            parents.append({"path": str(parent), "wakeup": state})
    result.update(
        usb_path=str(usb),
        usb_id=f"{read(usb / 'idVendor')}:{read(usb / 'idProduct')}",
        usb_remote_wake=remote_wake,
        usb_wakeup=wakeup,
        upstream_wakeup=parents,
    )
    result["candidate"] = remote_wake is True and wakeup in {"enabled", "disabled"}
    result["reason"] = "hardware-candidate" if result["candidate"] else "usb-wake-not-exposed"
    return result


def inventory():
    bus = dbus.SystemBus()
    manager = dbus.Interface(bus.get_object("org.bluez", "/"),
                             "org.freedesktop.DBus.ObjectManager")
    objects = manager.GetManagedObjects()
    adapters, devices = [], []
    for path, interfaces in objects.items():
        if "org.bluez.Adapter1" in interfaces:
            adapters.append(probe_adapter(str(path).rsplit("/", 1)[-1]))
        if "org.bluez.Device1" in interfaces:
            data = interfaces["org.bluez.Device1"]
            if not bool(data.get("Paired", False)):
                continue
            devices.append({
                "adapter": str(data.get("Adapter", "")).rsplit("/", 1)[-1],
                "address": str(data.get("Address", "")),
                "name": str(data.get("Alias", data.get("Name", ""))),
                "icon": str(data.get("Icon", "")),
                "modalias": str(data.get("Modalias", "")),
                "wake_allowed": bool(data["WakeAllowed"]) if "WakeAllowed" in data else None,
            })
    adapters.sort(key=lambda item: item["adapter"])
    devices.sort(key=lambda item: (item["adapter"], item["address"]))
    suspend_available = "mem" in (read(Path("/sys/power/state")) or "").split()
    for adapter in adapters:
        paired = [d for d in devices if d["adapter"] == adapter["adapter"]]
        adapter["selective_policy"] = (
            "observed" if any(d["wake_allowed"] is not None for d in paired)
            else "unknown-no-paired-devices" if not paired
            else "unavailable"
        )
        adapter["candidate"] = (
            adapter["candidate"] and adapter["selective_policy"] != "unavailable"
        )
    return {
        "suspend_available": suspend_available,
        "mem_sleep": read(Path("/sys/power/mem_sleep")),
        "bluez_selective_policy_observed": any(d["wake_allowed"] is not None for d in devices),
        "offer_setup_candidate": suspend_available and any(a["candidate"] for a in adapters),
        "verified_by_real_suspend": False,
        "adapters": adapters,
        "paired_devices": devices,
    }


if __name__ == "__main__":
    try:
        print(json.dumps(inventory(), indent=2, ensure_ascii=False))
    except dbus.DBusException as exc:
        raise SystemExit(f"No se pudo consultar BlueZ: {exc}") from exc
