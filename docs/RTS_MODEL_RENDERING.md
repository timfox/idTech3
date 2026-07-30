# RTS Model Rendering

The RTS module remains deterministic simulation code. It does not call renderer imports such as `RE_RegisterModel()` directly. Instead, client/game integration code owns model registration and passes the resulting `qhandle_t` into the RTS API.

## Flow

1. Register the model through the renderer:

```c
qhandle_t h = re.RegisterModel("models/rts/worker.dae");
```

2. Bind the path/handle to an RTS owner default or a specific entity:

```c
RTS_SetDefaultModelForOwner(RTS_OWNER_PLAYER1, "models/rts/worker.dae", h);
RTS_SetEntityModel(entityId, "models/rts/commander.glb", commanderHandle);
```

3. Build renderer-facing records each frame:

```c
rtsRenderEntity_t ents[128];
int count = RTS_BuildRenderEntities(ents, 128, terrainZ, worldUnitsPerCell);
```

4. Convert each `rtsRenderEntity_t` into a renderer `refEntity_t`:

```c
refEntity_t ref;
Com_Memset(&ref, 0, sizeof(ref));
ref.reType = RT_MODEL;
ref.hModel = ents[i].modelHandle;
VectorCopy(ents[i].origin, ref.origin);
AxisClear(ref.axis);
trap_R_AddRefEntityToScene(&ref);
```

`rtsRenderEntity_t::modelPath` is retained for diagnostics, hot reload, and late registration. `modelHandle == 0` means the entity has no registered render model yet and should be skipped or rendered with a fallback.

## Supported Assets

The renderer model path can target any format supported by `RE_RegisterModel()`, including `.dae` via the mesh import path. Collada support is intentionally renderer-owned; the RTS module only stores the path and opaque handle.
