# OpenHMD VR

Client integration for [OpenHMD](http://openhmd.net/) head-mounted displays: head tracking and software stereo eye separation on the existing Vulkan/SDL swapchain.

## Build

`USE_OPENHMD` defaults to **ON**. The client loads `libopenhmd.so.0` at **runtime** via `dlopen`, so the library is not required to compile.

```bash
# Runtime dependency (Ubuntu/Debian)
sudo apt install libopenhmd0

# Optional headers if you prefer linking later
sudo apt install libopenhmd-dev

# Disable at configure time
cmake -S . -B build-vk-Release -DUSE_OPENHMD=OFF ...
```

Vendored API header: `third_party/openhmd/openhmd.h` (OpenHMD 0.3.0, Boost 1.0).

## Enable at runtime

```text
vr_openhmd 1
ohmd_list
ohmd_open 0
ohmd_status
ohmd_recenter
```

Or `exec openhmd.cfg` after installing the library.

## Cvars

| Cvar | Default | Notes |
|------|---------|-------|
| `vr_openhmd` | `0` | Master enable; auto-opens `vr_openhmdDevice` when set |
| `vr_openhmdDevice` | `0` | Device index from `ohmd_list` |
| `vr_openhmdStereo` | `1` | Dual-eye `STEREO_LEFT`/`RIGHT` via `r_stereoSeparation` |
| `vr_openhmdMouse` | `0` | Allow mouse look while tracking |
| `vr_openhmdIpd` | `0` | Override IPD metres (`0` = device) |
| `vr_openhmdWorldScale` | `32` | Metres → Quake units for IPD mapping |
| `vr_openhmdFov` | `0` | When `1`, push HMD FOV into `cg_fov`/`fov` |
| `vr_openhmdRoll` | `1` | Apply HMD roll |

## Commands

| Command | Effect |
|---------|--------|
| `ohmd_status` | Library, device, IPD, angles |
| `ohmd_list` | Probe and list HMDs |
| `ohmd_open [index]` | Open device |
| `ohmd_close` | Close device |
| `ohmd_recenter` | Zero current orientation |

## Architecture

```
libopenhmd.so (dlopen)
  → OHMD_Frame (CL_Frame)
  → OHMD_ApplyViewAngles (CL_CreateCmd)
  → OHMD_WantStereo → SCR_UpdateScreen LEFT+RIGHT
  → r_stereoSeparation from IPD → R_SetupProjection
```

Display remains the mono SDL/Vulkan swapchain (side-by-side / anaglyph-friendly software stereo). Full distortion compositor and OpenXR submit are out of scope for this pass.

## Code

| File | Role |
|------|------|
| `runtime/client/platform/cl_openhmd.c` | dlopen, tracking, stereo policy |
| `runtime/client/platform/cl_openhmd.h` | Public API |
| `config/openhmd.cfg` | Convenience defaults |

## Related

- [STEAM.md](STEAM.md) — Steam Deck / Steam Input
- Classic `r_stereoSeparation` / `stereoFrame_t` in the Vulkan renderer
