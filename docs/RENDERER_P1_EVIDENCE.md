# Renderer P1 Evidence (Phase 1.6)

Evidence file: `render_cert/renderer_iq_p1.json`  
Commands: `iq_certification_export` · `iq_evidence_invalidate <dep>`

## Recorded fields

- build / device / extents
- profileHash · thresholdHash
- per-stage status, evidence type, observed metrics, failure reason

## Invalidation dependencies

| Token | Invalidates |
|-------|-------------|
| `bloom` / `bloom.frag` | bloom source, firefly, pyramid |
| `velocity` / `temporal` | velocity, history, reset, ghosting |
| `gbuffer` | G-buffer quant, material decode, lighting parity |
| `cluster` | cluster + lighting parity |
| `smaa` / `edge` | edge + SMAA |
| `threshold` | all stages |

Imported evidence from another GPU is display-only unless classified device-independent.

See [RENDERER_IQ_LIVE_CERTIFICATION.md](RENDERER_IQ_LIVE_CERTIFICATION.md).
