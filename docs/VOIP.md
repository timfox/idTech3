# VoIP

Proximity voice chat uses Opus (`USE_OPUS`) when **`cl_voip 1`**.

## Controls

| Cvar / command | Default | Purpose |
|----------------|---------|---------|
| `cl_voip` | `0` | Enable VoIP encode/decode |
| `cl_voipSend` | `0` | Set while transmitting (prefer `+voip`) |
| `+voip` / `-voip` | — | Push-to-talk |
| `cl_voipShowMeter` | `1` | HUD mic level while sending |
| `cl_voipLipFlap` | `1` | Drive head jaw/mouth morphs from voice power |
| `cl_voipLipFlapScale` | `4.0` | RMS → morph weight scale (clamped to 1) |
| `cl_voipLipFlapThresh` | `0.02` | Minimum RMS before flap applies |
| `cl_voipLipFlapDecay` | `0.85` | Power decay after speech stops |
| `cl_voipLipFlapMatch` | `80` | Max world-unit distance to match a model to a player |
| `cl_voipLipFlapMorph` | `jaw,mouthOpen,mouth_open,jaw_open` | Morph target names (IQM/glTF) |
| `cl_voipLipFlapRate` | `12` | Flap oscillation rate (Hz) while talking |
| `sv_voipProximity` | `1024` | Server relay range (`0` = global) |

## Lip flap

When a client is sending or receiving VoIP, the engine tracks per-client RMS power and, on each `AddRefEntityToScene`, applies morph weights to models near that player (body or approximate head height).

**Requirements:**

- `r_morph 1` (renderer morph system)
- Head/face mesh with blend shapes named in `cl_voipLipFlapMorph` (IQM or glTF `target_names`)
- Classic MD3 heads without morph targets are unchanged

Startup logs: `VoIP lip flap: enabled|disabled (morph …)`.

See also [MODEL_FORMATS.md](MODEL_FORMATS.md) (morph controls) and [GLTF.md](GLTF.md).
