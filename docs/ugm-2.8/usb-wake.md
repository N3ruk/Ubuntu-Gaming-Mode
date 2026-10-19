# Bluetooth-controller wake in UGM 2.8

**English** | [Español](../es/ugm-2.8/despertar-usb.md) · [2.8 overview](README.md)

When Bluetooth wake is accepted on a **fresh 2.8 installation**, UGM applies a
default-deny wake policy to physical USB devices exposing `power/wakeup`. It
keeps the `btusb` adapter and the required hub/PCI-controller path enabled for
Bluetooth wake. Unauthorized USB mice, keyboards, receivers and controllers
have `power/wakeup=disabled`. Paired Bluetooth controllers start with
`Device1.WakeAllowed=no`, including controllers paired before installing UGM.
The app authorizes controllers individually; it does not grant wake to other
USB devices.

A boot service reapplies the policy. One udev rule invokes the helper for each
USB device connected after installation; another prepares the wake path when a
`btusb` adapter is connected or reset. Root hubs are not denied because they
may be part of the shared path. UGM records each changed device's earlier
value, identity and boot ID; disabling the option or uninstalling attempts to
restore that value only if nobody changed it since.

**Upgrade behavior:** upgrading from UGM 2.5 does not enable this policy or
revoke existing permissions. Previous controller choices and wake paths are
retained. The strict policy is reserved for fresh 2.8 installations, including
when the option is enabled later through `dpkg-reconfigure`.

The rule controls the kernel's USB wake permission. BIOS/UEFI, ACPI, other
buses and third-party rules may provide additional wake sources. S3
compatibility and port isolation require a real suspend test on the target
hardware. This does not change physical power-button behavior or promise
power-on from S5.
