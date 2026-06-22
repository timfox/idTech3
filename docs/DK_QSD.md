# Domany–Kinzel quasi-stationary distribution (DK-QSD)

Research module implementing the bond directed-percolation line of the Domany–Kinzel automaton quasi-stationary distribution (QSD) via projected transfer-matrix power iteration, following Lee, Harada, and Kawashima ([arXiv:2606.11885](https://arxiv.org/abs/2606.11885)).

## Toggle

- **`cl_dk_qsd_enable` `1`** — register console commands and allow solves (default `0`).

Startup logs one line when the module registers.

## Build

Included with **`USE_RESEARCH_EXTENSIONS=ON`** (`IDTECH3_PROFILE=full` or `research`):

```bash
./scripts/compile_engine.sh vulkan full
cmake --build build-vk-Release --target unit_dk_qsd
ctest -R unit_dk_qsd
./tests/scripts/test_dk_qsd.sh
```

## Console

| Command | Description |
|---------|-------------|
| `dk_qsd_info` | Paper reference, critical point, limits |
| `dk_qsd_solve [N] [p] [chi]` | Solve QSD (dense if `N≤14`, else MPS) |
| `dk_qsd_obs` | Mean active count, R₁₁, half-chain MI, flock stats |
| `dk_qsd_sample` | One exact sample from cached QSD |
| `dk_qsd_mi` | Leading uniform-flock MI estimate (Eq. 4) |

Bond-DP line: `P[1]=p`, `P[2]=p(2−p)`, `p_c ≈ 0.644700185`.

## Implementation

| Path | Role |
|------|------|
| `src/extensions/research/dk_qsd/dk_qsd_kernels.c` | Local tensors **W**, **V** (Supplement S1) |
| `dk_qsd_dense.c` | Exact two-layer **T** for `N≤14`, **ΠT** power iteration |
| `dk_qsd_mps.c` | Probability MPS gate sweeps (Algorithm 1), perfect sampling (Algorithm 2) |
| `dk_qsd_observables.c` | ⟨n⟩, R₁₁, bipartite Shannon MI, flock extent/fill |
| `tools/dk_qsd/test_parity.py` | Python dense reference |

## Observables (inactive phase)

- **⟨n⟩** — O(1) active sites, N-independent.
- **R₁₁** — nearest-neighbor clustering ratio ≫ 1.
- **I(N/2, N)** — half-chain mutual information ≈ 1 bit (single flock positional bit).
- **Flock** — compact extent, high single-cluster weight deep in inactive phase.

## References

- Lee, Harada, Kawashima (2026) — information-theoretic QSD structure.
- Harada & Kawashima, PRL **123**, 090601 (2019) — prior DK MPS / absorbing-state entropy.
