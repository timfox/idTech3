"""x3DPRA configuration (Ma et al., arXiv:2606.06933)."""

from dataclasses import dataclass
from typing import Tuple

FREQ_GHZ = 2.4
LAMBDA0 = 0.125
K0 = 2 * 3.141592653589793 / LAMBDA0
C0_DB = 8.685889638  # 20*log10(e), matches X3DPRA_C0_DB in x3dpra.h

DOI_X = 0.9
DOI_Y = 0.9
DOI_Z = 0.3

VOXEL_NX = 60
VOXEL_NY = 60
VOXEL_NZ = 15

ZETA0 = 376.730313668
DIPOLE_R = 73.0

FRESNEL_DELTA_D = 0.2

OBJECTS = {
    "circle": {"eps_r": 10.0, "eps_i": 1.0, "alpha": 15.8, "height": 0.8},
    "square": {"eps_r": 8.0, "eps_i": 0.8, "alpha": 14.2, "height": 0.8},
    "two_cylinders": {"eps_r": 15.0, "eps_i": 1.5, "alpha": 19.5, "height": 0.25},
}


@dataclass
class OptConfig:
    tv_gamma: float = 0.04
    huber_tau: float = 0.05
    max_iter: int = 40
    step_size: float = 0.5
