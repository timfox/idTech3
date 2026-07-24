# Deferred material routing

Deferred eligibility is capability-based, not texture-count based. A material
enters Deferred only when every active opaque stage can be represented.
Destination blending, screen maps, portals/mirrors, distortion, unsupported
deforms/tcMods/animation, transmission/refraction, cinematics, and unsupported
advanced lobes route completely to Forward+ or a specialized pass.

No routed fallback writes a partial G-buffer. No later legacy stage may light a
Deferred-owned result again. Use `material_translate_status <material>`,
`deferred_status`, and `r_deferredEligibilityDebug` to inspect the decision.
Decals remain Forward-specialized until a DBuffer contract is certified.
