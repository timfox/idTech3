# Deep-layered machines (DLM)

Exact finite-depth distribution of the global Boolean output of a **deep-layered machine** (Fink, [arXiv:2606.11965](https://arxiv.org/abs/2606.11965)).

## Toggle

- **`cl_dlm_enable` `1`** — register console commands (default `0`).

## Build

Included with **`USE_RESEARCH_EXTENSIONS=ON`** (`IDTECH3_PROFILE=full` or `research`):

```bash
./scripts/compile_engine.sh vulkan full
cmake --build build-vk-Release --target unit_dlm
ctest -R unit_dlm
./tests/scripts/test_dlm.sh
```

## Console

| Command | Description |
|---------|-------------|
| `dlm_info` | Paper reference and usage |
| `dlm_q [k] [n]` | Exact Hamming-weight distribution q(n) via matrix A |
| `dlm_enum [k] [n]` | Exhaustive enumeration (small k,n) |
| `dlm_sample [k] [n] [samples]` | Monte Carlo weight histogram |
| `dlm_eigen [k]` | Eigenvalues λ_j = (ℓ)_j / ℓ^j |

## Mathematics

- ℓ = 2^k input configurations; output probability depends only on Hamming weight w ∈ {0,…,ℓ}.
- Transition (eq. 1): A_{i,j} = ℓ^{-ℓ} C(ℓ,j) i^j (ℓ-i)^{ℓ-j} with 0^0 = 1.
- q(n) = A^{n-1} q(1), q_i(1) = C(ℓ,i) / 2^{2^k}.
- Endpoints w=0 (false) and w=ℓ (true) absorb probability as n → ∞; crossover depth n_c ~ 2^k.

## Validation

Unit tests match **Table I** (k=1,n=2 and k=2,n=2) and exhaustive enumeration for k=1,n=2.

## References

- T. M. A. Fink (2026) — exact distribution and critical depth.
- Mozeika, Li, Saad PRL **125**, 168301 (2020) — infinite-depth constant limit.
