# WBOIT Source Normalization

`NormalizeOitSource(decodedSource, encoding, policy)` → `OitSurfaceSample`.

## Straight

```text
opacity = saturate(a)
unassociated = rgb
associated = rgb * opacity
```

## Premultiplied

```text
opacity = saturate(a)
associated = rgb
unassociated = (opacity > eps) ? rgb/opacity : 0
```

Near-zero α: no divide-by-zero; unassociated cleared; optional emissive keep of associated.

## Rejected for ordinary WBOIT

Additive, masked, multiplicative → `OIT_SAMPLE_FLAG_REJECTED` (routed before accum).

## Edge policy (`r_transparentEdgePolicy`)

| Value | Behavior |
|-------|----------|
| 0 | Preserve authored RGB |
| 1 | Zero RGB when α == 0 |
| 2 | Edge-safe normalization (certified straight) |
| 3 | Diagnostic only |

Production default: **0** (preserve).

See [TRANSPARENT_TEXTURE_AUTHORING.md](TRANSPARENT_TEXTURE_AUTHORING.md).
