# C++20 Conversion Order

**Status:** Foundation  
**See also:** [CPP20_MIGRATION.md](CPP20_MIGRATION.md), inventory [`cpp20_inventory.tsv`](cpp20_inventory.tsv)

## Classification

| Label | Meaning |
|-------|---------|
| `CPP_READY` | Compiles as C++20 with no or trivial fixes |
| `CPP_WITH_MECHANICAL_FIXES` | Needs casts / nullptr / compound-literal rewrite |
| `CPP_BLOCKED_BY_ABI` | Owns or mutates public ABI layouts |
| `CPP_BLOCKED_BY_C_ONLY_DEPENDENCY` | Depends on C-only headers or TUs not yet fixed |
| `KEEP_C_EXTERNAL_BOUNDARY` | Must remain C-callable façade forever (or until versioned ABI break) |
| `THIRD_PARTY_DO_NOT_CONVERT` | Vendored / upstream code |

## Global order (high level)

1. Unit tests & tools  
2. Isolated utilities (CRC/MD4/MD5, UTF-8, static Huffman, path helpers)  
3. Parsers / checksums / compression adapters  
4. Non-hot renderer utilities (cluster math, path ownership selectors, debug labels)  
5. qcommon leaves (endian helpers, info strings — **after** splitting large TUs)  
6. Renderer support (not device/swapchain/submit)  
7. Subsystem internals behind C façades  
8. Platform backends (one file at a time)  
9. Renderer core layers last (selection → cluster CPU → … → device last)

## Hard defer (do not convert in early milestones)

- `cm_trace.c`, `cm_load.c`, movement / pmove  
- `msg.c`, `net_chan.c`, VM (`vm*.c`)  
- Vulkan device, swapchain, command submit, descriptor allocator  
- Deferred lighting / cluster GPU build hot path  
- Native game module entry points  

## First batch (completed)

| File | Class | Milestone |
|------|-------|-----------|
| `engine/core/md4.cpp` | `CPP_WITH_MECHANICAL_FIXES` | foundation |
| `engine/core/md5.cpp` | `CPP_WITH_MECHANICAL_FIXES` | foundation |
| `engine/core/huffman_static.cpp` | `CPP_WITH_MECHANICAL_FIXES` | foundation |
| `engine/core/q_utf8.cpp` | `CPP_WITH_MECHANICAL_FIXES` | foundation |
| `renderers/vulkan/vk_cluster_math.cpp` | `CPP_READY` | foundation |

## Next recommended batch

| File | Class | Why |
|------|-------|-----|
| `engine/core/cm_bounds.c` | `CPP_READY` | Unit-tested, no net ABI |
| `runtime/client/core/cl_compat_math.c` | `CPP_READY` | Unit-tested leaf |
| `engine/core/huffman.c` | `CPP_WITH_MECHANICAL_FIXES` | After static tables |
| `renderers/vulkan/vk_render_path.c` | `CPP_WITH_MECHANICAL_FIXES` | C façade OK; not device lifecycle |

## Per-file conversion checklist

1. Record baseline unit-test output + `nm` symbols  
2. Rename / build as C++20  
3. Mechanical fixes only  
4. Preserve exported signatures + `extern "C"`  
5. Keep headers C-includable  
6. Re-run tests + symbol compare  
7. Update `cpp20_inventory.tsv` + `cpp20_status` counts  
8. Commit as `CPP20_FILE_CONVERSION` alone  

Stop the milestone if shared-header/ABI instability appears — fix the boundary first.
