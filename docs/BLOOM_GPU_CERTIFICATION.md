# Bloom GPU Certification (P1)

Live stages: `P1_CERT_BLOOM_SOURCE` · `P1_CERT_BLOOM_FIREFLY` · `P1_CERT_BLOOM_PYRAMID`  
Evidence: `GPU_READBACK` via IQ snapshot after bloom extract.

Contributor isolation, firefly attenuation/retention, and pyramid centroid/energy metrics are driven by `renderer_p1_certify core` / `iq_certify_core`.

SceneHDR must remain unmodified by the firefly filter. See [BLOOM_FIREFLY_CONTROL.md](BLOOM_FIREFLY_CONTROL.md) · [BLOOM_SOURCE_INTEGRITY.md](BLOOM_SOURCE_INTEGRITY.md).
