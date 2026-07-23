# Temporal History Registry (P1)

Owners: TAA, weapon, SSR, AO, volumetric, exposure, bloom, shadow, transparency, other.  
API: `vk_temporal_history_note` · `vk_temporal_history_unowned_active`.

Live stages: `TEMPORAL_HISTORY` · `TEMPORAL_RESET`.  
Failures: `UNOWNED_TEMPORAL_CONSUMER`, `TEMPORAL_RESET_NOT_OBSERVED`, stale generation/extent/projection.
