# TTP — Tree Traversal Prefetcher

Software characterization of the hardware prefetcher from Tozlu, Naithani & Zhou, *A Hardware-Efficient Design for Precise Prefetching in Ray Tracing* ([arXiv:2605.16253](https://arxiv.org/abs/2605.16253)).

Real TTP lives in **GPU RT units**: it reads node addresses already on each thread’s traversal stack and issues prefetches during **consecutive stack pops** (upward DFS traversal). Vulkan and this engine cannot access that stack; this module provides an **analytical model** and **synthetic BVH traversal simulator** aligned with the paper’s methodology (Vulkan-sim / Lumibench).

## Paper summary

| Mode | Trigger | Prefetch source |
|------|---------|-----------------|
| **DFS** | 1st / 2nd / 3rd+ consecutive pop after a push | Top 1, 2, or 16 stack entries (FSM in paper Fig. 8) |
| **BFS** | Every queue pop | Next *N* entries from queue head |

Reported path-tracing results (128×128, Lumibench): **~1.48×** geometric-mean speedup, **~98.9%** L1 prefetch accuracy, **~31.5%** L1 miss coverage vs baseline. TTP beats the Treelet prefetcher (MICRO 2023) without BVH layout changes.

## Console commands

```
ttp_status
ttp_sim dfs 12 6 42
ttp_sim bfs 14 6 1
ttp_lumibench
ttp_compare 14
ttp_bfs_sweep 14 6 7
```

## Cvars

| Cvar | Default | Purpose |
|------|---------|---------|
| `cl_ttp` | `1` | Register TTP commands; startup log when enabled |
| `cl_ttp_mem_wait` | `0.70` | Fraction of RT cycles waiting on BVH memory (paper Fig. 1) for speedup model |
| `cl_ttp_bfs_distance` | `4` | BFS prefetch distance *N* (paper §VI-C; `N=4` is the paper default) |

## Speedup model

Given estimated **miss coverage** *c* (fraction of baseline RT L1 misses turned into hits by TTP) and **memory wait fraction** *m*:

\[
\text{speedup} = \frac{1}{(1 - m) + m(1 - c)}
\]

Coverage comes from pop-streak histograms produced by `ttp_sim` / `ttp_lumibench`, with DFS FSM intensities **1 / 2 / 16** as in the paper. The software model is calibrated so DFS accuracy tracks the paper's reported **~98.9% L1 accuracy**, and BFS distance scaling follows the paper's **N=1 / 2 / 4** sweep.

## Calibration notes

- DFS mode preserves the synthetic traversal-derived coverage trend and calibrates issued-prefetch count to the paper's reported average L1 accuracy (**98.92%**).
- BFS mode uses the paper's reported average speedups for `N=1`, `N=2`, and `N=4` as anchor points, then interpolates or gently saturates beyond them.
- `ttp_bfs_sweep` is the quickest way to inspect those paper-aligned BFS distance tradeoffs in-engine.

## Limitations

- Not a hardware or driver prefetch implementation
- Synthetic 6-ary trees use Lumibench **depths** (Table II), not exact scene BVHs
- No Vulkan-sim cycle simulation; use [yavuz650/vulkan-sim](https://github.com/yavuz650/vulkan-sim) for reproduction of paper figures

## References

- Tozlu et al., arXiv:2605.16253 — TTP design and evaluation
- Chou et al., MICRO 2023 — Treelet prefetcher baseline
- Saed et al., MICRO 2022 — Vulkan-sim RT unit model
