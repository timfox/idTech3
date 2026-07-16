# Animgraph (chocolate layer)

JSON state machine subset for glTF/IQM clip selection without breaking QVM mods.

## Format (`animgraph/*.json`)

Tokenizer-friendly text (not strict JSON):

```
state idle 0 0.2
state run 1 0.15
```

- `state <name> <clipIndex> <blendSec>`

## Runtime

- [g_animgraph.c](../runtime/game/g_animgraph.c): `G_AnimGraph_Load`, `SetState`, `Update`
- Cvar `g_animgraph` (default 1)
- Lua: `Engine.AnimGraph.load`, `setState`, `update`
- Studio **Animation** panel: preview via ImGui Studio (extend `r_studio_tools`)

## Output

Drives `refEntity_t.frame` / `oldframe` / `backlerp` on the native game module path.

## Retarget / mocap (full-conversion pipeline)

| Tool | Purpose |
|------|---------|
| [tools/mocap_bvh_to_gltf.py](../tools/mocap_bvh_to_gltf.py) | BVH → glTF with skeleton |
| [scripts/retarget_skel.py](../scripts/retarget_skel.py) | Bone name mapping table → retargeted clip |

**Worked example:**

```bash
python3 tools/mocap_bvh_to_gltf.py capture.bvh -o models/hero_mocap.gltf
python3 scripts/retarget_skel.py --source capture.bvh --target models/hero.gltf --map retarget/hero_map.txt
```

Ship `animgraph/*.txt` clip indices that match the retargeted glTF clip order. Demo: `examples/demo_game/mod/animgraph/idle_run.txt`.
