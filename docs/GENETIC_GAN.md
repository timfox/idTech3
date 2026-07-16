# Genetic GAN — procedural body evolution API

The engine provides an **in-process genetic layer** (genome slots, crossover, mutation, fitness, phenotype proxies) plus an **optional external GAN decode hook** (genome JSON → `.glb`), mirroring the FLUX/TRELLIS out-of-process pattern.

Use this for creature editors, breeding loops, and evolution gameplay where a latent vector drives mesh morphs and behavior weights.

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `cl_geneticGan` | 0 | Master toggle |
| `cl_geneticGanDim` | 32 | Latent vector length (4–64) |
| `cl_geneticGanMutationRate` | 0.05 | Default mutation probability per gene |
| `cl_geneticGanCrossoverBlend` | 0.5 | Crossover blend (0=uniform swap, 1=always average parents) |
| `cl_geneticGanAsync` | 1 | Background SDL thread for decode |
| `cl_geneticGanAutoImport` | 1 | `RegisterModel` when decode completes |
| `cl_geneticGanRepo` | — | Path to your GAN checkout (`decode_genome.py`) |
| `cl_geneticGanPython` | python3 | Interpreter for wrapper |
| `cl_geneticGanCmd` | — | Shell template override |
| `cl_geneticGanTimeout` | 1800 | Warn if decode runs longer (seconds) |
| `cl_mlSerial` | 1 | One ML subprocess at a time (FLUX/TRELLIS/genome) |
| `cl_mlUseJobs` | 1 | Use engine `jobs_*` pool when `jobs_enabled 1` |
| `jobs_threads` | 0 (auto) | Worker count for job pool (cores − 1) |

ROM sync mirrors: `cl_geneticGanSyncJob`, `cl_geneticGanSyncSlot`, `cl_geneticGanSyncCount`, `cl_geneticGanSyncQueue`.

## Multithreading

Genome GAN decode uses the engine **job thread pool** (`Jobs_Submit`) when `cl_mlUseJobs 1` and `jobs_enabled 1`, with **SDL thread fallback** otherwise. Subprocess work runs off the main thread; **`RegisterModel` and cvar sync run on the main thread** via `Defer_Add` (flushed at the start of each `Com_Frame`).

- **`cl_mlSerial 1`** (default): only one ML pipeline job globally (coordinates with FLUX/TRELLIS when they adopt the same gate).
- **Decode queue**: up to 8 pending `genome_generate` slots; pumped automatically each frame when the worker is idle.
- Tune parallelism with `jobs_threads` (see job system startup log).

## Console workflow

```text
set cl_geneticGan 1
genome_create parent_a
genome_create parent_b
genome_fitness 0 0.8
genome_fitness 1 0.6
genome_breed 0 1 0.08 child_0
genome_phenotype 2
genome_generate 2
genome_decode_status
genome_view
genetic_gan_status
```

## Lua (`Engine.Genome`)

```lua
local a = Engine.Genome.create("parent_a")
local b = Engine.Genome.create("parent_b")
Engine.Genome.setFitness(a, 0.9)
Engine.Genome.setFitness(b, 0.4)
local child = Engine.Genome.breed(a, b, 0.05)
local pheno = Engine.Genome.getPhenotype(child)
-- pheno.bodyScale, pheno.morphWeights, ...
Engine.Genome.decode(child)  -- queues genome_generate
```

## Phenotype mapping (CPU, no ML)

Genes map deterministically to gameplay-facing scalars and up to eight morph weights (compatible with behavior-tree morph output):

| Gene index | Phenotype field |
|------------|-----------------|
| 0 | `bodyScale` |
| 1 | `limbLength` |
| 2 | `headSize` |
| 3 | `torsoWidth` |
| 4 | `agility` |
| 5 | `mass` |
| 0–7 | `morphWeights[]` |

Games can override or extend mapping in script; the GAN decode path is for high-fidelity mesh synthesis.

## External GAN hook

1. Place `decode_genome.py` in your repo (`cl_geneticGanRepo`).
2. Signature: `python decode_genome.py <genome.json> <output.glb> <slot>`
3. Without a repo script, `release/genetic_gan_decode.py` writes a **minimal placeholder GLB** scaled by gene 0 (smoke tests, no GPU).

Default command template:

```text
%P "%E/genetic_gan_decode.py" --repo "%R" --genome "%G" --output "%O" --slot %S %A
```

Tokens: `%R` repo, `%B` base, `%E` engine base (wrapper path), `%P` python, `%G` genome JSON, `%O` output GLB, `%S` slot, `%A` extra args.

## Build

`USE_GENETIC_GAN` (default ON). Disable with `-DUSE_GENETIC_GAN=OFF`.

Implementation:

- **Genome API** (crossover, mutation, phenotype, job status): [`modules/world/genetic_gan.cpp`](../modules/world/genetic_gan.cpp) — linked via qcommon when `USE_OPEN_WORLD` (client + dedicated server).
- **Async decode + model import**: [`extensions/generative/cl_genetic_gan.c`](../extensions/generative/cl_genetic_gan.c) — `genome_generate`, job queue, `RegisterModel` on main thread (requires `USE_GENETIC_GAN`, ON in `full` profile).

Tests:

```bash
./tests/scripts/test_genetic_gan.sh build-vk-Release
ctest -R 'test_genetic_gan|unit_genetic_gan'
./tests/scripts/test_cpp20_sources.sh   # migration revert guard
```

## Integration notes

- Decode jobs finalize on the **main thread** (`RegisterModel` after the worker exits).
- Pair with procedural animation (`ProcAnim`) and BT morph weights for locomotion after import.
- Chain with TRELLIS/FLUX for texture or refinement passes on decoded bodies if desired.
