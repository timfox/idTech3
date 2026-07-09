# Python scripting (embedded CPython)

Optional **embedded Python** scripting alongside **Lua**, **JavaScript (Duktape)**, and **C# (Mono)**. The design follows the **Infernux** batch + JIT ideas ([Chen, arXiv:2604.10263](https://arxiv.org/abs/2604.10263)): amortize the language boundary with **SoA batch read/write**, optional **Numba** JIT in user scripts.

Upstream reference engine: [Infernux](https://chenlizheme.github.io/Infernux/) (MIT, pybind11 + Vulkan core). This fork integrates a **CPython C-API** bridge in `qcommon` (no separate pybind11 module required for the core API).

## Build

```bash
# Debian/Ubuntu
sudo apt-get install python3-dev

./scripts/compile_engine.sh vulkan python
# or
cmake -S . -B build-vk-Release -DUSE_PYTHON=ON -DUSE_VULKAN=ON ...
```

**Default:** `USE_PYTHON=OFF` (CI and minimal builds unchanged).

Startup when enabled:

```
Python: runtime ready (3.12.x); py_reload scripts/python/*.py
Python: batch bridge enabled (Infernux-style SoA columns)
```

## Console commands

| Command | Description |
|---------|-------------|
| `py_reload [path.py ...]` | Initialize/reload scripts (empty args = init only) |
| `py_list` | Runtime status and tracked scripts |
| `py_dump` | Same as `py_list` |
| `py_exec <source>` | Run Python source string |

## Infernux benchmark commands (always on)

| Command | Description |
|---------|-------------|
| `infernux_model_status` | Help |
| `infernux_api` | Architecture summary |
| `infernux_model [single\|multi10\|multi100\|compute]` | Paper Tables I/II/IV FPS |
| `infernux_jit [N]` | Estimated Numba speedup vs plain Python |

## Script layout

| Path | Purpose |
|------|---------|
| `scripts/python/idtech3/engine.py` | `Engine` API wrapper |
| `scripts/python/idtech3/jit.py` | `infernux_jit` decorator (Numba optional) |
| `scripts/python/demo_wave.py` | Batch wave demo (repo reference) |
| `examples/demo_game/mod/scripts/python/` | Demo mod copies for `.pk3` |

Allowed load paths: `scripts/python/`, `gameplay/`, `server/`, `client/`

## Cvars

| Cvar | Default | Purpose |
|------|---------|---------|
| `py_autoInit` | `0` | Open CPython at startup |
| `py_allowEvents` | `1` | `Engine.on` event dispatch |
| `py_allowExec` | `1` | `Engine.exec` → console buffer |
| `py_frameCallbackBudgetMs` | `2` | Soft frame callback budget |
| `py_compatTarget` | `cpython-3.10+` | Read-only API label |
| `cl_infernux_model` | `1` | Paper benchmark commands |

## Native module `_idtech3`

Loaded automatically when the runtime starts:

- `print`, `cvar_get`, `cvar_set`, `exec`, `milliseconds`, `engine_info`
- `db_available`, `db_path`, `db_exec`, `db_query_one`
- `profile_set`, `profile_get`, `profile_delete`
- `on_frame(callback)`, `on_event(name, callback)`
- `batch_read(handles, field)`, `batch_write(handles, field, values)`
- `batch_info()`, `spawn_demo_grid(side, spacing)`

Fields: `position` / `pos`, `velocity` / `vel`

Database helpers target the engine profile DB (`save/engine_profile.db`) when SQLite support is compiled in.

## Example (batch + frame hook)

```python
from idtech3.engine import Engine, demo_handles

n = Engine.spawn_demo_grid(32, 2.0)
handles = demo_handles(n)

def on_frame(msec, real_msec):
    flat = Engine.batch_read(handles, "position")
    # ... update flat[1], flat[4], ... (Y components) ...
    Engine.batch_write(handles, "position", flat)

Engine.on("frame", on_frame)
```

Load in-game:

```
py_reload scripts/python/demo_wave.py
```

## JIT (optional Numba)

```bash
pip install numba
```

```python
from idtech3.jit import infernux_jit
import numpy as np

@infernux_jit(parallel=True)
def kernel(y, t):
    for i in range(len(y)):
        y[i] = np.sin(t + i * 0.1)
```

Without Numba, `infernux_jit` warns once and runs pure Python (Infernux §VI-B fallback).

## Events

Same bus as JavaScript/C# via `Com_ScriptEmitEvent`: `frame`, `map_load`, `menu_changed`, `entity_spawn`, `input_key`, etc.

Register with:

```python
Engine.on("map_load", lambda s0, s1, i0, i1: Engine.print(f"map {s0}"))
```

## Comparison to other scripting

| Language | Command | Build flag |
|----------|---------|------------|
| Lua | `script_reload` | `USE_LUA` (default ON) |
| JavaScript | `js_reload` | `USE_DUKTAPE` (default ON) |
| C# | `cs_reload` | `USE_CSHARP` (default OFF) |
| Python | `py_reload` | `USE_PYTHON` (default OFF) |

## Limitations (v1)

- Batch bridge uses engine-side SoA demo entities, not full scene transforms
- No pybind11 render-graph / component system (Infernux editor layer not ported)
- Numba JIT runs in Python only; no in-engine LLVM pipeline
- Dedicated server links Python when enabled (same as Lua/JS)

## See also

- [CSHARP.md](CSHARP.md)
- [AGENTS.md](../AGENTS.md)
- Chen, arXiv:2604.10263 — Infernux
