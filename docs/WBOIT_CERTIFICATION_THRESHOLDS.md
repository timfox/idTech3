# WBOIT Certification Thresholds (Phase 2.6)

Target attachment precision for production live gates (RGBA16F / R16F typical):

| Stage | Metric | Threshold |
|-------|--------|-----------|
| Empty pixel | modified empty pixels | **0** |
| Empty pixel | max AbsRGB vs `fog_scene` | ≤ 1e-3 (format) |
| Single layer | max AbsRGB vs source-over | ≤ 2e-3 |
| Revealage | Abs vs ∏(1−α) | ≤ 1e-3 |
| Revealage | additive contamination | **0** |
| Order stability | permutation variance (approved class) | documented per material |
| Fog | unexplained double-fog pixels | **0** |
| Additive | revealage delta during additive-only | **0** |
| Weights | finite, ≥ minWeight, ≤ maxWeight | contract freeze |
| Soak | anomalies / clear-resolve imbalance | **0** for 30+ min |

Material classification (order stability):

```text
WBOIT_GENERAL_APPROVED
WBOIT_APPROXIMATION_VISIBLE
WBOIT_SPECIALIZED_ROUTE_REQUIRED
```

High-error classes must route explicitly — do not force them to remain WBOIT.
