# Renderer P1 Thresholds (Phase 1.6)

**Module:** `vk_renderer_p1_thresholds.c`  
**Commands:** `iq_thresholds_status` · `iq_thresholds_export`  
**Export:** `render_cert/thresholds.json`

Changing thresholds changes `contractHash` and invalidates affected evidence (`iq_evidence_invalidate threshold`).

Do not widen thresholds to force a first live pass — identify the first divergent stage first.

Key defaults (see source for full set):

| Gate | Metric | Default |
|------|--------|---------|
| Firefly | false-positive estimate max | 0.25 |
| Firefly | spike attenuation min | 0.85 |
| Velocity | mean / max error | 1.5 / 4.0 |
| G-buffer | normal angular error max | 5° |
| G-buffer | roughness abs error max | 0.08 |
| Lighting | mean / max RGB error | 0.02 / 0.12 |
| Edge | spread width max | 4 px |
| Edge | contrast retention min | 0.35 |

Justified from render-target precision, analytical fixtures, and native-resolution IQ requirements.
