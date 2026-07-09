# BubbleSH — deformable bubble swarm dataset tools

Compact BubbleSH characterization utilities based on Ramesh et al., *BubbleSH: A Dataset of Rising Bubbles with Deformable Interfaces* ([arXiv:2607.07275](https://arxiv.org/abs/2607.07275)).

This engine integration does not try to reproduce the paper's front-tracking DNS. Instead, it exposes the paper's compact state layout and benchmark metrics in a lightweight in-engine form so we can reason about deformable bubble trajectories, spherical-harmonic state size, and evaluation scores without carrying the full mesh-based simulation.

## Paper-aligned state

- Bubble diameters: `4`, `5`, `6` mm
- Gas volume fractions: `5%` to `40%` in `5%` steps
- Bubbles per configuration: `32`
- Periodic cubic domain
- Shape descriptor: spherical harmonics up to order `L = 14`
- SH coefficient count: `(L + 1)^2 = 225`
- Per-bubble compact state: `position(3) + velocity(3) + SH coeffs`

The module also exposes the lower-order benchmark variants from the paper:

- `L = 3` -> `16` SH coefficients
- `L = 5` -> `36` SH coefficients
- `L = 14` -> `225` SH coefficients

## Console commands

```text
bubblesh_status
bubblesh_case 5 30 14
bubblesh_orders
bubblesh_metrics 0.12 0.20 0.80 0.04 0.30
bubblesh_w1_demo
```

- `bubblesh_status` — summary of dataset/tool defaults
- `bubblesh_case <diam_mm> <void_pct> [sh_order]` — report one BubbleSH configuration, including periodic box size and compact state dimension
- `bubblesh_orders` — print the paper-relevant SH orders and state sizes
- `bubblesh_metrics <mean_pos_err> <final_pos_err> <arc_length> <mean_chamfer> <surface_change>` — compute `R-ADE`, `R-FDE`, and `R-ACD`
- `bubblesh_w1_demo` — demo normalized 1-Wasserstein score using the paper's IQR normalization idea

## Metrics

The module implements the paper's normalized error style:

- Relative Average Displacement Error:
  `R-ADE = mean_displacement_error / arc_length`
- Relative Final Displacement Error:
  `R-FDE = final_displacement_error / arc_length`
- Relative Average Chamfer Distance:
  `R-ACD = mean_chamfer_distance / total_surface_change`
- Normalized `W1`:
  `W1(pred, gt) / IQR(gt)`

The Wasserstein normalization follows the paper's recommendation to normalize by the ground-truth interquartile range so physically different quantities can be compared on a dimensionless scale.

## Dataset notes

- The domain size is computed from the paper's periodic-box formula:
  `Lbox = (N * 4/3 * pi * r^3 / eps)^(1/3)`
- Temporal resolution follows the table in the paper:
  `5 mm` cases use `1e-4 s`; `4 mm` and `6 mm` cases use `1e-3 s` at `eps = 15, 25, 35`, otherwise `1e-4 s`
- The compact state is geared toward emulator evaluation, not direct renderer playback

## References

- Ramesh et al., arXiv:2607.07275 — BubbleSH dataset, compact SH state, and evaluation metrics
