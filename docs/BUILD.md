# Build & Run

This repository is CMake-based, but the **recommended** path is the wrapper script in `tools/`.

## Build (recommended)

- **OpenGL (Release)**:

```bash
bash tools/compile_engine.sh opengl Release
```

- **Vulkan (Release)**:

```bash
bash tools/compile_engine.sh vulkan Release
```

- **Debug**:

```bash
bash tools/compile_engine.sh opengl Debug
```

Build outputs land in renderer-specific build dirs (`build-gl-<type>` / `build-vk-<type>`) and are copied into `release/`.

## Run

This repo does **not** ship Quake III Arena content. To actually play, you need game data (e.g. `pak0.pk3`) in:

- `release/base/`

### Client

```bash
cd release
./idtech3.x86_64
```

### Dedicated server

```bash
cd release
./idtech3.server.x86_64 +set dedicated 1
```

### Helpful startup flags

- **Set base/home paths explicitly** (useful when launching from elsewhere):

```bash
./idtech3.x86_64 +set fs_basepath . +set fs_homepath .
```

- **Use command-line `+` commands** (supported):

```bash
./idtech3.server.x86_64 +set fs_basepath . +set fs_homepath . +set dedicated 1 +quit
```

