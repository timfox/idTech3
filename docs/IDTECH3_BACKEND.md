# idTech3 Backend submodule

Optional Git submodule for game/server backend logic maintained separately from the engine tree.

| Item | Value |
|------|--------|
| **Repository** | [timfox/idtech3backend](https://github.com/timfox/idtech3backend) |
| **Path** | `src/external/idtech3backend` |
| **Build** | Not linked into `idtech3` by default (stub repo; wire CMake/game modules when content lands) |

## Initialize

```bash
git submodule update --init src/external/idtech3backend
# or:
./scripts/init_optional_submodules.sh --backend
./scripts/init_optional_submodules.sh --all
```

Clone with submodules:

```bash
git clone --recurse-submodules <idtech3-repo-url>
```

## Using from the engine tree

After init, backend sources live at:

```
${ENGINE_ROOT}/src/external/idtech3backend/
```

Point custom game builds, scripts, or future `USE_IDTECH3_BACKEND` CMake wiring at that path. The engine does not require this submodule for a default Vulkan client/server build.

## Update pin

```bash
cd src/external/idtech3backend
git fetch origin && git checkout <commit>
cd ../..
git add src/external/idtech3backend
```

Commit the updated submodule pointer in the parent repo when you bump the backend revision.
