# Vector Font Winding

Based on Lengyel JCGT 2017 robust quadratic classification.

## Algorithm

1. Translate curves so the sample is at the origin.
2. Fire horizontal and vertical analytical rays.
3. Solve quadratic/ray intersections (`calcRootCode` + poly solve).
4. Accumulate signed fractional coverage contributions.
5. Parameter domain treated as open at `t=1` via root classification table `0x2E74`.

## Dual sorted lists

- **X-sorted** list (descending max-x): horizontal-ray early exit.
- **Y-sorted** list (descending max-y): vertical-ray early exit.

Previously a single max-x sort was reused for both axes, which weakened vertical early-exit and increased per-pixel curve tests.

## Nearly-linear quadratics

When `|a| < 1/65536`, the solver falls back to the linear parameterization (`rb`).

## Fill rule

Non-zero winding via absolute coverage mix of X and Y contributions (Lengyel dual-axis AA).
