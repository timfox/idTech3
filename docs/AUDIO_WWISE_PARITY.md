# Wwise-Inspired OpenAL Runtime Mixer

Runtime features inspired by [Wwise 2026.1 Sound Engine](https://www.audiokinetic.com/en/public-library/2026.1.1_9196/?source=SDK&id=whatsnew_2026_1_new_features.html) concepts. This is **not** a Wwise integration; it maps familiar authoring/runtime ideas onto the existing OpenAL + EFX stack.

## Implemented (runtime)

| Wwise concept | Engine mapping |
|---------------|----------------|
| Output buses | `s_bus_sfx`, `s_bus_ui`, `s_bus_voice`, `s_bus_music`, `s_bus_amb` |
| RTPCs | `s_rtpc_gameIntensity`, `s_rtpc_combat`, `snd_setrtpc` |
| State groups | `gameplay` (default/combat/paused), `location` (default/underwater/interior) via `snd_setstate` or `s_mixer_state_*` |
| Auto-ducking | `s_mixer_duck_*` lowers music/SFX/amb when voice plays |
| Sound events | `sound/soundevents.txt` or `scripts/soundevents.txt`, `snd_playevent` |
| Replay capture | `s_mixer_replay_enable`, `snd_replay_dump` CSV |
| Max propagation distance | `s_mixer_propagation_max` (cuts EFX/occlusion past range) |
| Reflection effort | `s_acoustics_reflection_effort` scales probe ray count |

## Console

```
snd_mixer_info
snd_playevent ui_click
snd_setstate location underwater
snd_setrtpc combat 0.8
snd_replay_dump mixer_replay.csv
snd_mixer_reload
```

## Sound events file

```
# name bus volume samplePath
explosion_small sfx 1.0 sound/weapons/rocketfly.wav
menu_back ui 0.8 sound/misc/menu4.wav
```

## Cvars (startup log when enabled)

- `s_mixer_enable` — master toggle (default `1`)
- Bus gains: `s_bus_*`
- Duck: `s_mixer_duck_enable`, `s_mixer_duck_amount`, attack/release ms
- Spatial: `s_mixer_propagation_max`
- Replay: `s_mixer_replay_enable`, `s_mixer_replay_capacity`

## Not in scope (authoring / middleware)

- Wwise Authoring, banks, Unity/Unreal plugins
- Timeline/segment unified authoring (use `snd_music_adaptive.c` + Lua director)
- Full diffraction/path graph (heuristic rays only)

See also [AUDIO.md](AUDIO.md).
