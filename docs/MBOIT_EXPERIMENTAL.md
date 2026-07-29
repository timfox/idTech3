# MBOIT Experimental (Phase 2.6)

`r_oit 2` remains **experimental**. It does **not** inherit `WBOIT_PRODUCTION_CERTIFIED`.

Compare via `r_mboitCompare` / `mboit_certification_status` against sorted reference and WBOIT.

Current hardening status:

- Pass 1 accumulates optical-depth power moments and total optical depth `b0`.
- Pass 2 uses moment transmittance for weighted transparent accumulation.
- Resolve now uses MBOIT optical-depth coverage, `coverage = 1 - exp(-b0)`, when `r_oit 2` is active.
- Debug views: `r_oitDebug 20` shows MBOIT coverage from `b0`; `r_oitDebug 21` shows first-moment mean depth.
- CPU reference: `unit_mboit_resolve`.

MBOIT failures must not block WBOIT certification. Promotion requires independently passing the same live gates.
