# AIWC — Architecture-Independent Workload Characterization

CPU-side implementation of metrics from Chilukuri, Milthorpe & Johnston, *Characterizing Optimizations to Memory Access Patterns using Architecture-Independent Program Features* (IWOCL 2020), inspired by the [AIWC Oclgrind plugin](https://github.com/BeauJoh/aiwc).

This is **not** Oclgrind integration; it records **virtual** memory addresses from OpenCL-style kernel simulators and computes architecture-independent features useful for GPU memory optimization.

## Metrics

| Metric | Description |
|--------|-------------|
| **Total / 90% footprint** | Unique addresses; smallest set covering 90% of accesses |
| **Global / local MAE** | Shannon entropy of global vs local address distributions |
| **LMAE (n bits dropped)** | Entropy after dropping n LSBs of each address (n = 0..10) |
| **Relative local usage** | Fraction of accesses to `__local` / shared memory |
| **Parallel spatial locality (PSL)** | Per-timestep entropy of simultaneous work-group accesses, averaged over timesteps and work-groups (paper §5) |

## Console commands

```
aiwc_status
aiwc_matmul simple 256
aiwc_matmul coalescedABT 256
aiwc_matmul_all 256
```

## Cvar

- `cl_aiwc` (default `1`) — startup log line when AIWC commands are registered

## Matrix multiply validation suite

Simulates the paper's incremental OpenCL matmul kernels:

- `simple` — baseline global loads
- `coalescedA` — tile A in local memory
- `coalescedAB` — tile A and B
- `coalescedABT` — transposed A tile (bank-conflict mitigation)
- `alignedABT` — same access pattern as ABT (alignment is hardware-specific)

Run `aiwc_matmul_all 256` and compare **PSL at 10 bits dropped**: `coalescedABT` should show a steeper drop than `simple`, matching paper Figure 4 trends.

## Limitations

- Symbolic **simulation**, not SPIR-V / GLSL instrumentation
- Matmul suite only (OpenDwarfs benchmarks not yet ported)
- No ML performance prediction (Johnston et al. HPCS 2018) in this pass

## References

- Chilukuri et al., IWOCL 2020 — parallel spatial locality metric
- Johnston & Milthorpe, LLVM-HPC 2018 — AIWC feature set
- NVIDIA CUDA C++ Best Practices Guide — matmul optimization motivation
