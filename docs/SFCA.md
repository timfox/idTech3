# Separable-Field Cellular Automaton (SFCA)

Normalized rank-one row–column field CA from Shi & Huang — survival/birth interval geometry, long transients, and basin competition between cycle families.

Reference: [Dynamics in a Low-Rank Separable Field Cellular Automaton](https://github.com/huangmengs/SFCA) (upstream simulation code).

## Toggle

- **`cl_sfca_enable` `1`** — register console commands (default `0`).

## Build

Included with **`USE_RESEARCH_EXTENSIONS=ON`** (`IDTECH3_PROFILE=full` or `research`):

```bash
./scripts/compile_engine.sh vulkan full
cmake --build build-vk-Release --target unit_sfca
ctest -R unit_sfca
./tests/scripts/test_sfca.sh
```

## Model

On a periodic `H × W` lattice, row/column occupancies are blurred (3-point periodic wrap), combined as `n = R_j C_i`, normalized `q = n / n_max`. Alive cells survive when `q ∈ S`; dead cells birth when `q ∈ B`.

Four outcome classes (finite window): extinction, fixed point, cycle, long transient.

Interval geometry: **`B ⊆ S`**, partial overlap, or no overlap — organizes fixed-point vs long-transient ridges (Fig. 3).

## Console

| Command | Description |
|---------|-------------|
| `sfca_info` | Module reference |
| `sfca_run [maxGen]` | Single trajectory (representative rule, 75×100) |
| `sfca_batch [n]` | Outcome shares over `n` runs |
| `sfca_phase` | Coarse `wS × wB` sample with Δ_low=5/18 |
| `sfca_transition` | Canonical axis scan (`S_low=10/180`, `B=[60/180,160/180]`) |
| `sfca_fingerprints` | Dense vs sparse cycle branches (wS=55 vs 70) |
| `sfca_survival` | Kaplan–Meier S(t) for ordered vs critical wS |
| `sfca_damage` | Damage-spreading plateau dH vs wS |

Representative rule (Fig. 2): `S = [3/18, 11/18]`, `B = [7/18, 9/18]`.

## API highlights

| Function | Paper mapping |
|----------|----------------|
| `SFCA_ScanWidthCellBatch` | Phase diagram cell (Fig. 3) |
| `SFCA_ScanTransitionAxis` | Long-transient ridge (Fig. 5) |
| `SFCA_CycleFingerprints` | Density / χ / stripe (Fig. 6) |
| `SFCA_KaplanMeier` | Transient survival curves (Fig. 5, S6) |
| `SFCA_ScanDamageAxis` | Basin-competition damage (Fig. 7) |
| `SFCA_FieldStdDevMean` | Normalized-field fluctuations (Fig. 8) |

Grid cap: **8192** cells; history depth **4096** generations.

## Validation

`unit_sfca` checks core dynamics, interval geometry, multi-class outcomes, transition ridge, cycle fingerprint separation (high χ on low-wS side), Kaplan–Meier survival, and damage spreading.
