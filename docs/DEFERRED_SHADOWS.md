# Deferred shadow parity

Deferred and Forward+ bind the shared shadow contract and must agree on sun
direction/radiance, cascade selection, receiver and normal bias, PCF kernel,
depth convention, and compare operation. A simplified Deferred shadow is not a
production fallback.

`shadow_parity_status` reports static wiring. `shadow_parity_validate` cannot
pass production certification without matching scene-linear per-pixel shadow
term readbacks.
