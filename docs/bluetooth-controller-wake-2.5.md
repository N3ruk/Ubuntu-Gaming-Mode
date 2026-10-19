# Bluetooth-controller wake in UGM 2.5 (historical)

**English** | [Español](es/despertar-mando-bluetooth.md) · [Documentation index](README.md)

## Demonstrated scope

On the reference MSI MPG Z390 GAMING PLUS (MS-7B51) with Intel USB Bluetooth
`8087:0aaa` (`btusb`), Linux 7.0.0-38, BlueZ 5.85 and two paired DualShock 4
controllers, one controller woke the PC from S3 (`deep`) suspend. BIOS PCIe and
USB wake were enabled. The USB adapter, hub and XHCI controller all had
`power/wakeup=enabled`, and ACPI XHC was enabled. This validates **that
combination**, not arbitrary motherboards, adapters or firmware.

## Install-time inventory

`ugm-bt-wake probe` reports:

- `/sys/power/mem_sleep`, board, `btusb` adapters, VID:PID and PCI controller;
- adapter/hub/XHCI `power/wakeup` and path ACPI state;
- BlueZ/D-Bus, Python D-Bus, GTK and Polkit availability, plus paired,
  trusted HID controllers exposing `Device1.WakeAllowed`;
- `candidate` when a USB path and `deep` suspend exist; `eligible` when
  required software also exists; `ready_to_test` when current state permits a
  test. `validated` remains false without a recorded physical test.

`candidate` does not establish BIOS support. Debconf shows a notice on USB HCI
and asks whether to enable Bluetooth and show the optional wake app. No
controller is authorized automatically; `missing` lists absent dependencies.

The 2.5 app was in a **single `.deb`**, default-off. BlueZ and app-only packages
were recommended, not hard dependencies. With APT recommendations disabled or
`dpkg -i`, they might be absent. Accepting the option enabled and started
`bluetooth.service` when available and prepared the USB wake path; failure
warned without aborting UGM. Rejecting left the service unchanged. The option
could be changed with `sudo dpkg-reconfigure ubuntu-gaming-mode`.
Noninteractive installs used defaults. For **current 2.8 installation**, see
[the installation guide](installation.md) and [new USB policy](ugm-2.8/usb-wake.md).

## App and persistence

The “UGM · Bluetooth controller wake” app toggles each paired HID controller.
Polkit authorization is used only to change `Device1.WakeAllowed`; the app
shows the actual value and records each choice in
`/var/lib/ubuntu-gaming-mode/bt-wake.json`. `ugm-bt-wake-apply.service`
reapplies the USB path at boot, independently of controller choices.
`ugm-bt-wake-watch.service` sets newly paired HID controllers to
`WakeAllowed=no`; known controllers retain their choice. On removal/purge,
UGM attempts to restore values only where they still match UGM's last write,
preserving and reporting external conflicts.

Acceptance requires selecting one controller, rebooting, checking the probe,
suspending manually in `deep`, then verifying wake by its PS button without
using the physical power button. Repeat disabled and with a second controller;
then test removal/reinstallation.

## Can it power on from shutdown?

This implementation covers S3, **not S5 or G3**. In S5 neither BlueZ nor the
OS is running; `WakeAllowed=yes` alone is insufficient. Some platforms support
S4/S5 USB wake with standby power and capable firmware/device, but that needs
specific physical testing and cannot be promised for this Intel/DS4 setup.
From G3, without standby power, Bluetooth wake is impossible without external
hardware.
