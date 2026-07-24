# Deferred production certification

The contract is versioned by `deferredRenderingContract_t`. Changing G-buffer
quality/encoding, normal encoding, ownership encoding, BRDF, cluster indexing,
lightmap decode, shadow filtering, AO ownership, emissive ownership, or the
opaque composite changes its hash and invalidates evidence.

Certification levels progress from `DEFERRED_UNCERTIFIED` through
`DEFERRED_STATIC_READY` to `DEFERRED_PRODUCTION_CERTIFIED`. Static wiring and a
successful build can reach only `DEFERRED_STATIC_READY`. Production requires
current GPU scene-linear HDR readback evidence, non-empty fixtures, lifecycle
coverage, zero invalid/double owners, zero fullbright escapes, and passing
component/final error thresholds.

Commands:

```text
deferred_contract_status
deferred_contract_validate
deferred_certify
deferred_certify_status
deferred_certify_abort
deferred_certify_resume
deferred_parity_metrics
deferred_parity_report
deferred_parity_certify
```

Certification is automatically invalidated by a contract, G-buffer generation,
or cluster generation change. The current implementation deliberately reports
missing GPU evidence instead of promoting source-code identity to image parity.
