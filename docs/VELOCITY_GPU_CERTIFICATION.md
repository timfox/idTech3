# Velocity GPU Certification (P1)

Stage: `P1_CERT_VELOCITY`  
Compares decoded GPU motion to analytical expectations when the motion buffer is allocated.  
Under the IQ reference profile (`r_taa 0`) an absent motion buffer is recorded as valid evidence (native reference disables TAA/MV).

Gate name: `VELOCITY_GPU_CERTIFIED` (via live stage PASS). Do not proceed to temporal promotion when velocity fails with an allocated buffer.
