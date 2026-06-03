# Test data

| File | Description |
|------|-------------|
| `fog_2cubed.nvdb` | Minimal 2³ NanoVDB FogVolume blind grid (values 1–8). Regenerate: `python3 tests/scripts/gen_minimal_nvdb.py` (blind) or `python3 tests/scripts/gen_minimal_nvdb.py --leaf tests/data/leaf_8cubed.nvdb`

**CI:** `unit_nanovdb_decode` decodes blind, leaf, and this file (via `TEST_DATA_DIR`). |

In-game: copy into your `base/` PK3 or folder, then `vdb_load fog_2cubed.nvdb`.
