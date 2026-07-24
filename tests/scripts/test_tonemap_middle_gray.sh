#!/usr/bin/env bash
# CPU check: corrected filmic maps middle gray above crush threshold.
set -euo pipefail
python3 - <<'PY'
def partial(x, toe_pow, s):
    m = max(x, 0.0) ** toe_pow
    return m / (m + s)

def filmic(x, toe=0.10, shoulder=0.30, wp=1.5):
    toe_pow = 1.0 + (2.4 - 1.0) * toe
    s = 0.45 + (2.6 - 0.45) * shoulder
    return partial(x, toe_pow, s) / partial(wp, toe_pow, s)

def filmic_old_bug(x, toe=0.18, shoulder=0.28, wp=2.5):
    toe_pow = 1.0 + (2.4 - 1.0) * toe
    s = 0.45 + (2.6 - 0.45) * shoulder
    t = max(x / wp, 0.0)
    mapped = t ** toe_pow
    nw = 1.0 / (1.0 + s)
    return (mapped / (mapped + s)) / nw

mid_new = filmic(0.18)
mid_old = filmic_old_bug(0.18)
assert filmic(0.0) < 1e-6, "black must stay black"
assert mid_new >= 0.12, f"midgray too dark: {mid_new}"
assert mid_old < 0.10, f"regression probe: old bug should crush ({mid_old})"
assert mid_new > mid_old * 1.5, "fix must lift midtones vs old WP predivide"
print(f"test_tonemap_middle_gray: ok mid_new={mid_new:.4f} mid_old={mid_old:.4f}")
PY
