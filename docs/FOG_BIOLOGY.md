# Fog bioaerosol ecology (Evans et al. 2019)

Engine-side model of **fog microbial aerosol** dynamics from [Evans et al., *Science of The Total Environment* 647 (2019)](https://doi.org/10.1016/j.scitotenv.2018.08.045). Implements the paper’s key mechanistic drivers as a toggleable chocolate-layer simulation — not a metagenomics pipeline.

## What the paper found (mapped to engine)

| Finding | Engine representation |
|---------|----------------------|
| Coastal fogs contain diverse bacteria/fungi distinct from clear aerosols | `FOG_BIO_PHASE_CLEAR` / `FOG` / `POST_FOG` community profiles |
| Marine microbes dominate near coast, soil phyla inland | `r_fogBiologyCoastKm`, `r_fogBiologyWindMarine` → `marineFraction`, `oceanOtuFraction` |
| Fog increases viable deposition (~3× culturable growth, ~2× richness) | `depositionMultiplier`, `culturableRichness` in fog phase |
| Fog raises alpha diversity vs clear air | Shannon index higher in `FOG_BIO_PHASE_FOG` |
| Maine vs Namib site differences | Presets `r_fogBiologySite` 0 (Maine) / 1 (Namib 50 km inland) |
| Pre/post-fog compositional shifts | Bacteroidetes drop in Maine fog; Ascomycota enrichment in Namib fog |
| Gram-negative dominance (Proteobacteria + Bacteroidetes) | `gramNegativeFraction` |
| Rhodospirillales higher in fog vs clear | `rhodospirillalesFraction` (Maine ~7% fog / 0.2% clear) |
| Ocean OTU fraction 1–75% in fog | `oceanOtuFraction` |
| Pathogen-associated genera (Table 1) | `pathogenTaxaScore` + `fog_biology_genera` |
| Marine orders (Oceanospirillales, Pseudoalteromonas, etc.) | Boost via `FB_ApplyMarineTaxa` on Proteobacteria/Cyanobacteria |

**Field sites (paper):** Maine — Southport Island, within ~30 m of ocean (`r_fogBiologyCoastKm 0.03`). Namib — Gobabeb / Uniab, ~50 km inland (`demo_fog_biology_namib.cfg`).

**Sequence data:** SRA [SRP155760](https://www.ncbi.nlm.nih.gov/sra/?term=SRP155760) — not reproduced in-engine.

Dominant phyla modeled: **Proteobacteria**, **Bacteroidetes**, **Actinobacteria**, **Firmicutes**, **Cyanobacteria**, **Ascomycota** (Namib fungi).

## Cvars (default off)

| Cvar | Default | Role |
|------|---------|------|
| `r_fogBiology` | `0` | Enable fog bioaerosol ecology tick |
| `r_fogBiologySite` | `0` | `0` = Maine coastal, `1` = Namib inland |
| `r_fogBiologyCoastKm` | `10` | Distance from coast (km) — marine influence falloff |
| `r_fogBiologyWindMarine` | `0.8` | Onshore marine wind component 0–1 |
| `r_fogBiologyAuto` | `1` | Treat `r_volumetricFog 1` as active fog for phase machine |
| `r_fogBiologyCoastAuto` | `0` | Derive `r_fogBiologyCoastKm` from player distance to coast origin |
| `r_fogBiologyCoastAxis` | `0` | Axis for coast auto (`0`=X, `1`=Y) |
| `r_fogBiologyCoastOrigin` | `0` | World coordinate of coastline |
| `r_fogBiologyCoastUnitsPerKm` | `512` | World units per km for coast auto |

Startup when enabled: `[fog_biology] enabled site=… coast_km=… auto=…`

## Console

| Command | Purpose |
|---------|---------|
| `fog_biology_status` | Current phase, marine influence, phylum abundances |
| `fog_biology_compare` | Maine coastal vs Namib 50 km inland fog communities |
| `fog_biology_poll` | One-line live snapshot (phase, marine, shannon, deposition, pathogen) |
| `fog_biology_sweep` | Print marine influence vs coast distance samples |
| `fog_biology_genera` | Dominant genera from Evans Table 1 (site-dependent) |
| `fog_biology_paper` | Replication report: model vs published trends |

### Sync mirror cvars (read-only, updated each frame when enabled)

| Cvar | Role |
|------|------|
| `r_fogBiologySyncPhase` | `clear` / `fog` / `post_fog` |
| `r_fogBiologySyncMarine` | Marine influence 0–1 |
| `r_fogBiologySyncShannon` | Shannon diversity |
| `r_fogBiologySyncDeposition` | Deposition multiplier |
| `r_fogBiologySyncPathogen` | Pathogen deposition risk 0–1 |
| `r_fogBiologySyncCoastKm` | Current coast distance (km) |
| `r_fogBiologySyncOceanOtu` | Ocean OTU fraction 0–1 |
| `r_fogBiologySyncGramNeg` | Gram-negative fraction |
| `r_fogBiologySyncRhodo` | Rhodospirillales proxy |

Used by the Vulkan **Bioaerosol Ecology** ImGui panel without linking `fog_biology.c` into the renderer.

## Integration

```
CL_GameFrame → FogBiology_Frame()
             → phase from r_volumetricFog / manual fog state
             → community metrics for gameplay / Lua / telemetry hooks
```

Query API (C):

- `FogBiology_GetCurrentCommunity()`
- `FogBiology_GetMarineInfluence()`
- `FogBiology_GetPathogenDepositionRisk()` — heuristic 0–1 from deposition × marine × diversity
- `FogBiology_GetPhase()`

### Lua (`Engine.FogBiology`)

```lua
if Engine.FogBiology.enabled() then
  local c = Engine.FogBiology.getCommunity()
  local risk = Engine.FogBiology.getPathogenRisk()
  if risk > 0.6 then
    -- coastal fog deposition event: boost marine spawner weight, etc.
  end
end
Engine.FogBiology.setSite("namib")
Engine.FogBiology.setCoastKm(50)
Engine.FogBiology.setMarineWind(0.7)
```

| Function | Returns |
|----------|---------|
| `enabled()` | boolean |
| `getPhase()` | `"clear"`, `"fog"`, `"post_fog"` |
| `getMarineInfluence()` | number 0–1 |
| `getPathogenRisk()` | number 0–1 |
| `getCommunity([phase])` | table `{ shannon, marine, oceanOtu, deposition, richness, gramNegative, rhodospirillales, pathogenTaxa, phyla={...} }` |
| `setSite("maine"\|"namib"\|0\|1)` | — |
| `setCoastKm(km)` | — |
| `setMarineWind(0..1)` | — |
| `setFogActive(bool)` | — |

### Telemetry

When `r_fogBiology 1`, each frame records:

- `fog_bio_phase` (0=clear, 1=fog, 2=post_fog)
- `fog_bio_coast_km`
- `fog_bio_ocean_otu`
- `fog_bio_gram_neg`
- `fog_bio_pathogen_taxa`
- `fog_bio_marine`
- `fog_bio_shannon`
- `fog_bio_deposition`
- `fog_bio_pathogen_risk`

Read via `Engine.Telemetry.get("fog_bio_pathogen_risk")`.

### ImGui

**Volumetrics** inspector → **Bioaerosol Ecology** section (cvar toggles + preset).

## Example preset

```text
exec demo_fog_biology.cfg
exec demo_fog_biology_namib.cfg
exec demo_fog_biology_openworld.cfg
fog_biology_paper
fog_biology_genera
fog_biology_poll
/lua demo_fog_biology()
```

Or manually:

```text
set r_fogBiology 1
set r_fogBiologySite 0
set r_fogBiologyCoastKm 0.5
set r_volumetricFog 1
fog_biology_status
fog_biology_compare
```

## Testing

```bash
ctest -R unit_fog_biology -V
ctest -R test_fog_biology -V
```

## Related

- [VDB_WOODCOCK_VOLUMETRICS.md](VDB_WOODCOCK_VOLUMETRICS.md) — volumetric fog rendering
- [VOLUMETRIC_FOG_ENHANCEMENTS.md](VOLUMETRIC_FOG_ENHANCEMENTS.md)

## Scope limits

This module does **not** implement 16S/ITS sequencing, culturing, PerMANOVA, or NMDS from the paper. It encodes **published ecological drivers** for game/simulation use. For research reproduction, use the authors’ SRA accession **SRP155760** and supplementary methods.
