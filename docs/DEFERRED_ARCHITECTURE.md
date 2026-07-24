# Deferred rendering architecture

`r_deferredArchitecture` freezes the opaque-lighting architecture independently
from debug presentation:

| Mode | Contract |
|---:|---|
| 0 | Forward+ reference |
| 1 | Legacy hybrid additive Deferred |
| 2 | Full-fidelity material Deferred production target |
| 3 | Deferred versus Forward+ comparison |
| 4 | Strict lighting-ownership validation |

Mode 0 is the parity reference. Mode 1 preserves the old
`SceneBaseLit + DeferredDynamic` migration path, but is not a production
target. Modes 2–4 use true unlit material exports and owner-based replacement
for eligible pixels. Materials that cannot be represented completely remain
Forward+ or specialized from start to finish.

The architecture cvar is latched. Changing it requires `vid_restart`, which
also invalidates G-buffer, depth, cluster, comparison, and any future temporal
evidence. Temporal SSR remains quarantined regardless of architecture.

Commands:

```text
deferred_architecture_status
deferred_architecture_validate
lighting_ownership_status
lighting_ownership_validate
```

Full-fidelity/strict validation requires `r_gbufferQuality 2`.

Production certification is evidence-driven. Static wiring, successful
compilation, or a zero counter before geometry was submitted is not sufficient
evidence of Forward+/Deferred image parity.
