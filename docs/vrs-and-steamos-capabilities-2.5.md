# VRS and display capabilities in UGM 2.5.0-5 (historical)

**English** | [Español](vrs-y-capacidades-steamos.md) · [Documentation index](README.md)

This describes the stable **2.5** integration, not the changes introduced in
[2.8 VRS](ugm-2.8/vrs.md). Results depend on GPU, driver and game.

## HDR and VRR

When the user's preference is `auto` or `on`, UGM announces the compositor's
general HDR/VRR capability to Steam; `off` disables that path. Gamescope
3.16.30 separately reports the active connector capability through Xwayland
properties `GAMESCOPE_DISPLAY_SUPPORTS_HDR` and `GAMESCOPE_VRR_CAPABLE`,
updating them when output changes. It does not make a fixed EDID decision
before Steam launches. Inventory is diagnostic, not a substitute for checking
the active connector. Steam GamepadUI is closed source; control visibility can
vary between monitors.

## Install-time VRS selection

`ugm-vrs-select` requires an unambiguous DRM-to-single-identifiable-hardware-
Vulkan-GPU relationship, suitable driver/ICD and pipeline VRS. It saves the
route in `/etc/ubuntu-gaming-mode/vrs.conf`:

| Route | Condition | Installation |
| --- | --- | --- |
| `radv` | Integrated RADV APU with a PCI ID on the selector's conservative list | Automatic; native Mesa dynamic file, no UGM layer |
| `nvidia` | Proprietary NVIDIA with VRS features and available layer | Experimental Debconf question, default no |
| `amd` | Eligible AMD RADV not taking native APU route | Experimental Debconf question, default no |
| `none` | Ambiguous/ineligible GPU or rejected layer | No dynamic VRS announcement |

The presence of `VK_KHR_fragment_shading_rate` alone does not force VRS.
Custom layers also check device/driver capability in-process. The AMD layer
is not designed for AMDVLK or AMD's proprietary Vulkan driver. Reinstall or
reconfigure to reevaluate selection after a GPU or driver change.

## Components and Steam toggle

The historical build compiled AMD/NVIDIA layers at 32 and 64 bits under
`prototypes/{amd,nvidia}-vrs/` and stored manifests in
`/usr/lib/ubuntu-gaming-mode/vrs-bundled`, without global Vulkan registration.
Private install state under `/var/lib/ubuntu-gaming-mode` used mode 0700.
After Gamescope starts, before Steam, `post_gamescope_start` stages the chosen
layer in `~/.local/share/ubuntu-gaming-mode/vrs` from the **readable package
payload**; it does not try to read private-state copies as the normal user.
Native RADV needs no UGM layer. Failed staging suppresses VRS announcement.

For a working route, the hook creates an initial `1x1` state file under
`~/.local/state/ubuntu-gaming-mode`, exports `STEAM_USE_DYNAMIC_VRS=1` and
`RADV_FORCE_VRS_CONFIG_FILE`, and for custom layers also exports
`UGM_VRS_CONFIG_FILE`, `VK_ADD_LAYER_PATH` and `VK_INSTANCE_LAYERS`. Steam's
toggle should write `1x1`/`2x2`. Because Steam is closed source, visibility
cannot be guaranteed without live GamepadUI inspection. Previously recorded
command buffers do not change until rerecorded by the game.

The NVIDIA layer passed 32/64-bit and offscreen differential rendering tests
on RTX 2060. AMD and RADV paths passed selection and integration tests, **not
physical AMD GPU validation**. Forced VRS can affect image quality; layers
promise neither higher performance nor compatibility with all games/pipelines.

References: [Mesa RADV VRS variables](https://docs.mesa3d.org/envvars.html#envvar-RADV_FORCE_VRS),
[Khronos `VK_KHR_fragment_shading_rate`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_fragment_shading_rate.html),
[Gamescope HDR/VRR](https://github.com/ValveSoftware/gamescope),
[vkd3d-proton D3D12 VRS tests](https://github.com/HansKristian-Work/vkd3d-proton/blob/master/tests/d3d12_vrs.c).
