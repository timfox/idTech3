# x3DPRA physics symbol map (Ma et al., arXiv:2606.06933)

| Paper | Code (C) | Code (Python) | Notes |
|-------|----------|---------------|-------|
| λ₀ | `X3DPRA_LAMBDA0_M` | `LAMBDA0` | 0.125 m @ 2.4 GHz |
| k₀ | `X3dpra_Wavenumber()` | `K0` | 2π/λ₀ |
| C₀ (dB) | `X3DPRA_C0_DB` | `C0_DB` | 20·log₁₀(e) ≈ 8.686 |
| Im{G(r)} | `X3dpra_ScalarGreen3D` | `scalar_green_im` | sin(kr)/(4πr) |
| ψ (Im) | `X3dpra_KernelPsi` | `kernel_psi` | Dipole-normalized Rytov kernel |
| Wₗₙ | `X3dpra_WeightEntry` | `weight_entry` | C₀·k₀·Im(ψ) |
| Fresnel mask | `X3dpra_FresnelMask` | `fresnel_mask` | r_mt+r_mr < r_link+δ |
| Δα | reconstruction unknown | `alpha` / `delta_alpha` | Voxel attenuation contrast |
| y (links) | `x3dpra_link_meas_t` | `forward_measurements` | Phaseless RSS after bg sub |

Eq. 15 background subtraction: `ΔP = P_obj − P_bg` → `tools/x3dpra/rss_io.py`.

Eq. 28 optimization: `min ½‖y − Wα‖² + γ TV(α)` → `tval3.py` / `optimize.py`.
