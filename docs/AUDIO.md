# Audio Systems

## Backends

| Backend | File | Description |
|---------|------|-------------|
| OpenAL | `snd_backend_openal.c` | Primary 3D audio with HRTF and EFX reverb |
| SDL | `snd_backend_sdl.c` | Fallback stereo audio |
| Null | `snd_backend_null.c` | Silent backend for dedicated servers |

## Codecs

| Format | File | License |
|--------|------|---------|
| WAV | `snd_codec_wav.c` | Built-in |
| MP3 | `snd_codec_mp3.c` | Built-in (minimp3) |
| Ogg Vorbis | `snd_codec_ogg.c` | Vendored (libogg) |
| Opus | `snd_codec_opus.c` | Vendored (libopus + opusfile) |
| FLAC | `snd_codec_flac.c` | Vendored (libFLAC) |
| WebM | `snd_codec_webm.c` | Vendored (nestegg + opus) |

## Spatial Audio

- **HRTF:** `s_openalHrtf` cvar enables head-related transfer functions
- **EFX Reverb:** `s_openalEfx` with presets (generic, room, hall, underwater, etc.)
- **Geometry Acoustics:** `snd_acoustics_efx.c` -- ray-traced room estimation using `CM_BoxTrace`, auto-applies reverb and occlusion filters based on environment geometry
- **Doppler:** `s_openalDopplerFactor` for moving sound sources

**Cvars:** `s_acoustics_enable`, `s_acoustics_debug`, `s_acoustics_rays`, `s_acoustics_occlusion_enable`

## Adaptive Music (`snd_music_adaptive.h/c`)

Intensity-driven layered music system. Driven by the AI Director.

### Layers (8 max)
Each layer has an intensity range. Layers auto-crossfade based on `Director_GetGlobalIntensity()`:
```lua
Engine.Music.addLayer("music/ambient.ogg",  0, 0.0, 0.3, 0.5)  -- calm
Engine.Music.addLayer("music/tension.ogg",  1, 0.3, 0.6, 0.5)  -- building
Engine.Music.addLayer("music/combat.ogg",   2, 0.6, 1.0, 0.5)  -- action
```

### Stingers (16 max)
One-shot cues triggered at intensity thresholds:
```lua
Engine.Music.addStinger("music/sting_danger.ogg", 0.8, 10.0, true)
```

### Global Controls
```lua
Engine.Music.setIntensity(0.5)     -- manual override
Engine.Music.fadeToSilence(3.0)    -- fade out over 3 seconds
```
