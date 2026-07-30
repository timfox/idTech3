# Day/Night World Lighting

`r_dayNight 1` enables renderer-owned real-time day/night lighting. The system captures the map-authored `q3map_sun` direction and radiance as the noon baseline after world load, then updates the canonical renderer sun each frame:

- `tr.sunDirection`
- `tr.sunLight`

Existing passes already consume those fields, so the cycle reaches physical sky/atmosphere, volumetric fog, cascaded sun shadows, deferred sun BRDF, entity lighting, and RTX/Hybrid paths through the normal lighting spine.

## Controls

| Cvar | Default | Notes |
| --- | --- | --- |
| `r_dayNight` | `0` | Opt-in; classic maps are unchanged by default. |
| `r_dayNightUseRealTime` | `1` | Use local wall-clock time. |
| `r_dayNightTime` | `12` | Manual hour when real time is off and no accelerated cycle is active. |
| `r_dayNightCycleMinutes` | `0` | Non-zero runs a full 24-hour cycle over this many minutes. |
| `r_dayNightLatitude` | `35` | Tilts the sun path. |
| `r_dayNightNorthYaw` | `0` | Rotates the sun path in world yaw degrees. |
| `r_dayNightSunScale` | `1` | Daytime authored sun multiplier. |
| `r_dayNightMoonScale` | `0.035` | Night directional light fraction. |
| `r_dayNightAmbientScale` | `0.18` | Twilight fill around sunrise/sunset. |

## Usage

```cfg
exec vulkan_overlay_day_night.cfg
daynight_status
```

For a fast validation loop:

```cfg
seta r_dayNightUseRealTime 0
seta r_dayNightCycleMinutes 2
```

The overlay enables `r_skyOwner 1` and `r_atmosphere 1` so the sun motion is visible immediately. The lighting system itself does not require physical sky; classic skyboxes still keep their authored textures while world lighting follows the cycle.
