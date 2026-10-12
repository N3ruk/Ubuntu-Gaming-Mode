#!/usr/bin/env python3
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DETECTOR = ROOT / "src/ugm-capabilities"
VK_NVIDIA = """GPU0:
 vendorID = 0x10de
 deviceID = 0x1f08
 deviceType = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
 deviceName = NVIDIA RTX 2060
 driverID = DRIVER_ID_NVIDIA_PROPRIETARY
 deviceUUID = gpu-nvidia
GPU1:
 vendorID = 0x10005
 deviceID = 0x0000
 deviceType = PHYSICAL_DEVICE_TYPE_CPU
 deviceName = llvmpipe
"""
VK_AMD = """GPU0:
 vendorID = 0x1002
 deviceID = 0x73bf
 deviceType = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
 deviceName = AMD
 driverID = DRIVER_ID_MESA_RADV
 deviceUUID = gpu-amd
"""

VK_INTEL = """GPU0:
 vendorID = 0x8086
 deviceID = 0x56a0
 deviceType = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
 deviceName = Intel
 driverID = DRIVER_ID_INTEL_OPEN_SOURCE_MESA
 deviceUUID = gpu-intel
"""
VK_VRS_TRUE = """GPU0:
 deviceUUID = gpu-nvidia
 VK_KHR_fragment_shading_rate : extension revision 2
 VkPhysicalDeviceFragmentShadingRateFeaturesKHR:
 pipelineFragmentShadingRate = true
"""
VK_VRS_FALSE = """GPU0:
 deviceUUID = gpu-nvidia
 VkPhysicalDeviceFragmentShadingRateFeaturesKHR:
 pipelineFragmentShadingRate = false
"""


class CapabilityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.drm = self.root / "sys/class/drm"
        self.drm.mkdir(parents=True)
        self.summary = self.root / "vulkan.txt"
        self.summary.write_text(VK_NVIDIA)
        self.details = self.root / "vulkan-details.txt"
        self.details.write_text(VK_VRS_TRUE)
        self.card("card1", "10de", "1f08", "0000:01:00.0")
        self.connector("card1-HDMI-A-1", "connected")
        self.connector("card1-DP-1", "disconnected")

    def card(self, name, vendor, device, address):
        pci = self.root / "pci" / address
        pci.mkdir(parents=True)
        (pci / "vendor").write_text("0x" + vendor)
        (pci / "device").write_text("0x" + device)
        path = self.drm / name
        path.mkdir()
        (path / "device").symlink_to(pci)

    def connector(self, name, status):
        path = self.drm / name
        path.mkdir()
        (path / "status").write_text(status)
        return path

    def run_detector(self, **kwargs):
        args = [str(DETECTOR), "--sys-root", str(self.root),
                "--vulkan-summary", str(self.summary),
                "--vulkan-text", str(self.details)]
        if kwargs.get("preferred"):
            args += ["--prefer-vk-device", kwargs["preferred"]]
        if kwargs.get("session"):
            args += ["--session"]
        result = subprocess.run(args, text=True, capture_output=True, check=True)
        return result.stdout.splitlines() if kwargs.get("session") else json.loads(result.stdout)

    def test_single_gpu_and_software_renderer(self):
        data = self.run_detector()
        self.assertEqual(data["relation"]["state"], "matched")
        self.assertEqual(data["output"]["gpu_pci_address"], "0000:01:00.0")
        self.assertEqual(data["capabilities"]["radv_vrs"]["state"], "absent")
        self.assertEqual(data["capabilities"]["vrs_hardware"]["state"], "present_unvalidated")
        self.assertEqual(data["capabilities"]["nvidia_native_vrs"]["state"], "present_unvalidated")
        self.assertEqual(data["capabilities"]["nvidia_layer_vrs"]["state"], "present_unvalidated")
        self.assertEqual(self.run_detector(session=True)[0], "HDMI-A-1,*")
        self.assertEqual(data["output_candidates"], ["HDMI-A-1"])
        self.assertEqual({c["connector"]: c["status"] for c in data["connectors"]},
                         {"DP-1": "disconnected", "HDMI-A-1": "connected"})

    def test_missing_vulkan_is_unknown(self):
        self.summary.write_text("")
        data = self.run_detector()
        self.assertEqual(data["relation"]["state"], "unknown")
        self.assertEqual(data["capabilities"]["hdr"]["state"], "unknown")

    def test_contradictory_gpu_is_mismatched(self):
        self.summary.write_text(VK_AMD)
        data = self.run_detector()
        self.assertEqual(data["relation"]["state"], "mismatched")
        self.assertEqual(data["capabilities"]["radv_vrs"]["state"], "unknown")
        self.assertEqual(self.run_detector(session=True)[0], "HDMI-A-1,*")

    def test_multiple_gpus_require_selection(self):
        self.summary.write_text(VK_NVIDIA + VK_AMD)
        self.assertEqual(self.run_detector()["relation"]["state"], "unknown")
        data = self.run_detector(preferred="gpu-amd")
        self.assertEqual(data["relation"]["state"], "mismatched")

    def test_display_change_invalidates_unambiguous_output(self):
        (self.drm / "card1-DP-1" / "status").write_text("connected")
        data = self.run_detector()
        self.assertEqual(data["relation"]["state"], "unknown")
        self.assertEqual(data["output_candidates"], ["DP-1", "HDMI-A-1"])
        self.assertEqual(self.run_detector(session=True)[0], "DP-1,HDMI-A-1,*")

    def test_moving_display_from_hdmi_to_dp_promotes_dp(self):
        (self.drm / "card1-HDMI-A-1" / "status").write_text("disconnected")
        (self.drm / "card1-DP-1" / "status").write_text("connected")
        data = self.run_detector()
        self.assertEqual(data["output"]["connector"], "DP-1")
        self.assertEqual(data["output_candidates"], ["DP-1"])
        self.assertEqual(data["relation"]["state"], "matched")
        self.assertEqual(self.run_detector(session=True)[0], "DP-1,*")

    def test_display_identity_changes_without_reusing_validation(self):
        path = self.drm / "card1-HDMI-A-1" / "edid"
        edid = bytearray(256)
        edid[:8] = bytes.fromhex("00ffffffffffff00")
        edid[126] = 1
        edid[128] = 2
        edid[130] = 9
        edid[132:136] = bytes((0xE3, 0x06, 0x04, 0x01))
        path.write_bytes(edid)
        first = self.run_detector()
        self.assertEqual(first["capabilities"]["hdr"]["state"], "present_unvalidated")
        self.assertNotEqual(first["capabilities"]["hdr"]["state"], "validated")
        edid[10] = 1
        path.write_bytes(edid)
        second = self.run_detector()
        self.assertNotEqual(first["output"]["display_id"], second["output"]["display_id"])
        self.assertEqual(second["capabilities"]["hdr"]["state"], "present_unvalidated")

    def test_amd_radv_route_requires_vulkan_features(self):
        self.summary.write_text(VK_AMD)
        self.details.write_text(VK_VRS_TRUE.replace("gpu-nvidia", "gpu-amd"))
        self.card("card2", "1002", "73bf", "0000:02:00.0")
        (self.drm / "card1-HDMI-A-1" / "status").write_text("disconnected")
        self.connector("card2-HDMI-A-1", "connected")
        self.assertEqual(self.run_detector()["capabilities"]["radv_vrs"]["state"],
                         "present_unvalidated")
        self.details.write_text(VK_VRS_FALSE.replace("gpu-nvidia", "gpu-amd"))
        self.assertEqual(self.run_detector()["capabilities"]["radv_vrs"]["state"], "absent")
        self.details.write_text("")
        self.assertEqual(self.run_detector()["capabilities"]["radv_vrs"]["state"], "unknown")

    def test_attachment_only_vrs_is_hardware_present_but_not_radv_pipeline_route(self):
        self.summary.write_text(VK_AMD)
        self.details.write_text("""GPU0:
 deviceUUID = gpu-amd
 VK_KHR_fragment_shading_rate : extension revision 2
 pipelineFragmentShadingRate = false
 primitiveFragmentShadingRate = false
 attachmentFragmentShadingRate = true
""")
        self.card("card2", "1002", "73bf", "0000:02:00.0")
        (self.drm / "card1-HDMI-A-1" / "status").write_text("disconnected")
        self.connector("card2-HDMI-A-1", "connected")
        data = self.run_detector()["capabilities"]
        self.assertEqual(data["vrs_hardware"]["state"], "present_unvalidated")
        self.assertEqual(data["radv_vrs"]["state"], "absent")

    def test_nvidia_attachment_only_does_not_enable_forced_pipeline_route(self):
        self.details.write_text("""GPU0:
 deviceUUID = gpu-nvidia
 VK_KHR_fragment_shading_rate : extension revision 2
 pipelineFragmentShadingRate = false
 primitiveFragmentShadingRate = false
 attachmentFragmentShadingRate = true
""")
        data = self.run_detector()["capabilities"]
        self.assertEqual(data["nvidia_native_vrs"]["state"], "present_unvalidated")
        self.assertEqual(data["nvidia_layer_vrs"]["state"], "absent")

    def test_intel_route_absent_even_if_vulkan_vrs_present(self):
        self.summary.write_text(VK_INTEL)
        self.details.write_text(VK_VRS_TRUE.replace("gpu-nvidia", "gpu-intel"))
        self.card("card2", "8086", "56a0", "0000:02:00.0")
        (self.drm / "card1-HDMI-A-1" / "status").write_text("disconnected")
        self.connector("card2-HDMI-A-1", "connected")
        data = self.run_detector()
        self.assertEqual(data["capabilities"]["vrs_hardware"]["state"], "present_unvalidated")
        self.assertEqual(data["capabilities"]["radv_vrs"]["state"], "absent")
        self.assertEqual(data["capabilities"]["nvidia_native_vrs"]["state"], "absent")

    def test_driver_identity_missing_is_unknown_for_radv(self):
        self.summary.write_text(VK_NVIDIA.replace(" driverID = DRIVER_ID_NVIDIA_PROPRIETARY\n", ""))
        data = self.run_detector()
        self.assertEqual(data["capabilities"]["radv_vrs"]["state"], "unknown")

    def test_session_does_not_invoke_vulkaninfo(self):
        marker = self.root / "vulkan-called"
        fake_bin = self.root / "bin"
        fake_bin.mkdir()
        command = fake_bin / "vulkaninfo"
        command.write_text(f"#!/bin/sh\ntouch {marker}\nexit 99\n")
        command.chmod(0o755)
        env = {**os.environ, "PATH": str(fake_bin) + ":" + os.environ["PATH"]}
        result = subprocess.run([str(DETECTOR), "--session", "--sys-root", str(self.root)],
                                env=env, text=True, capture_output=True, check=True)
        self.assertEqual(result.stdout.splitlines()[0], "HDMI-A-1,*")
        self.assertFalse(marker.exists())

    def test_cached_inventory_invalidates_on_display_change(self):
        env = {**os.environ, "XDG_CACHE_HOME": str(self.root / "cache")}
        args = [str(DETECTOR), "--sys-root", str(self.root),
                "--runtime-version", "2.5-dev", "--vulkan-summary", str(self.summary)]
        refreshed = json.loads(subprocess.run(args + ["--refresh"], env=env, text=True,
                                              capture_output=True, check=True).stdout)
        self.assertEqual(refreshed["inventory_status"], "refreshed")
        cached = json.loads(subprocess.run(args + ["--cached"], env=env, text=True,
                                           capture_output=True, check=True).stdout)
        self.assertEqual(cached["inventory_status"], "cached_unverified")
        self.assertEqual((self.root / "cache/ubuntu-gaming-mode/inventory-v1.json").stat().st_mode & 0o777, 0o600)
        upgraded = json.loads(subprocess.run([str(DETECTOR), "--sys-root", str(self.root),
                                                "--runtime-version", "2.5-next", "--cached"],
                                               env=env, text=True, capture_output=True, check=True).stdout)
        self.assertEqual(upgraded["inventory_status"], "stale")
        (self.drm / "card1-DP-1" / "status").write_text("connected")
        stale = json.loads(subprocess.run(args + ["--cached"], env=env, text=True,
                                          capture_output=True, check=True).stdout)
        self.assertEqual(stale["inventory_status"], "stale")
        self.assertEqual(stale["relation"]["state"], "unknown")
        self.assertEqual(stale["output_candidates"], ["DP-1", "HDMI-A-1"])

    def test_no_connector_is_unknown(self):
        (self.drm / "card1-HDMI-A-1" / "status").write_text("disconnected")
        self.assertEqual(self.run_detector()["capabilities"]["vrr"]["state"], "unknown")
        self.assertEqual(self.run_detector(session=True)[0], "*")


if __name__ == "__main__":
    unittest.main()
