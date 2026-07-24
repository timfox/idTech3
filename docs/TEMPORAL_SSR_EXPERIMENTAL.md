# Temporal SSR quarantine

Temporal SSR is the confirmed owner of the geometry-shaped silhouette ghost
artifact. It is not part of the stable renderer or IQ-reference contract.

Production policy:

```text
r_ssr 0, or r_ssr 1 using the current-frame-only raster path
r_ssrTemporal 0
r_temporalSSR 0
r_allowExperimentalTemporalSSR 0
```

The current Vulkan SSR pass reads current SceneHDR and current depth, writes
its current-frame result, and owns no history image. `r_ssrTemporal` is a
research request, while `r_allowExperimentalTemporalSSR` is the independent
permission gate. Enabling the request without permission is rejected.

Even with research permission, the present implementation is deliberately a
non-operative certification marker: no temporal SSR image is allocated or
sampled and no temporal result modifies SceneHDR. This prevents the previously
observed artifact from contaminating Deferred/Forward+ comparisons while a
future research implementation is developed.

Commands:

```text
ssr_temporal_status
ssr_temporal_validate
temporal_history_status
```

Any future temporal implementation must reset on camera cuts, teleports, FOV
or projection changes, resize/render-scale changes, `vid_restart`, map restart,
view-cluster changes, Deferred/Forward architecture changes, and depth
convention changes before it may leave this quarantine.
