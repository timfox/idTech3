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

- [g_animgraph.c](../src/game/g_animgraph.c): `G_AnimGraph_Load`, `SetState`, `Update`
- Cvar `g_animgraph` (default 1)
- Lua: `Engine.AnimGraph.load`, `setState`, `update`
- Studio **Animation** panel: preview via ImGui Studio (extend `r_studio_tools`)

## Output

Drives `refEntity_t.frame` / `oldframe` / `backlerp` on the native game module path.
