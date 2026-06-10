# idTech3 Backend submodule

Optional Git submodule for game/server backend logic maintained separately from the engine tree.

| Item | Value |
|------|--------|
| **Repository** | [timfox/idtech3backend](https://github.com/timfox/idtech3backend) |
| **Path** | `src/external/idtech3backend` |
| **Build** | Not linked into `idtech3` by default; **App CRDT** auto-wires when `app_crdt/manifest.json` exists |

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

## App CRDT integration (engine-native)

When `app_crdt/manifest.json` is present in the submodule, CMake defines `IDTECH3_BACKEND_DIR` and the engine:

| Feature | Cvar / API |
|---------|------------|
| Auto-publish on map load | `com_app_crdt_auto 1` (default) |
| Backend root detection | `com_app_crdt_backend 1` (default) |
| Override root | `com_app_crdt_backend_root /path/to/backend` |
| Server Lua API | `Engine.AppCrdt.publish`, `emit`, `getVersion`, `isEnabled` |
| Client Lua API | `Engine.AppCrdt.emit` (via server relay) |

Layout:

```text
src/external/idtech3backend/
├── app_crdt/manifest.json      # semver + script list
└── server/lua/backend_app.lua  # server-side hotload script
```

Enable:

```bash
./release/idtech3_server +set dedicated 1 +set com_app_crdt 1 +map mymap
```

See [APP_CRDT.md](APP_CRDT.md) for wire format and manual test steps.

## Using from the engine tree

After init, backend sources live at:

```
${ENGINE_ROOT}/src/external/idtech3backend/
```

Optional strict mode: `-DUSE_IDTECH3_BACKEND=ON` fails configure if the submodule is missing (for CI that requires backend content).

## Update pin

```bash
cd src/external/idtech3backend
git fetch origin && git checkout <commit>
cd ../..
git add src/external/idtech3backend
```

Commit the updated submodule pointer in the parent repo when you bump the backend revision.
