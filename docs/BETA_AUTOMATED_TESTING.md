# Automated gameplay beta testing

Chocolate-layer support inspired by Hernández Bécares, Costero Valero, and Gómez Martín (*An approach to automated videogame beta testing*, Entertainment Computing 18, 2017).

The engine is not fully component-based like the paper’s Unity testbed, but the same ideas apply:

1. **Record** per-frame `usercmd_t` streams plus optional **high-level events** (JSON Lines).
2. **Replay** raw commands for regression / compatibility after code changes.
3. **Test** with success / fail patterns and `max_time_ms` (immediate failure conditions from the paper).
4. **Petri net models** (data files) describe level logic for future high-level adaptive replay; see `examples/demo_game/beta_traces/`.

## Toggle and logging

| Cvar | Default | Purpose |
|------|---------|---------|
| `cl_betaTrace` | `1` | Master enable (set `0` to discourage recording in production builds) |
| `cl_betaTraceLog` | `1` | Startup log line; `2` logs every `beta_event` |

## Console commands

| Command | Description |
|---------|-------------|
| `beta_record <basename>` | Start recording to `beta_traces/<basename>.betacmd` and `.betaevt` |
| `beta_stop` | Stop recording; writes `.betatest` manifest |
| `beta_play <basename>` | Replay `.betacmd` (injects recorded usercmds; live input suppressed) |
| `beta_test <basename>` | Replay + evaluate manifest success/fail patterns |
| `beta_event <type> [source] [target]` | Log a high-level event (mods, scripts, debug) |
| `beta_mark_success <pattern>` | While recording, add success pattern to manifest |
| `beta_mark_fail <pattern>` | While recording, add fail pattern to manifest |

Files are written under the game home path, e.g. `~/.idtech3/beta_traces/` (same search path as demos).

## File formats

### `*.betacmd` (raw input trace)

Text, one usercmd per line:

```
# idtech3 betacmd v1
<serverTime> <pitch> <yaw> <roll> <buttons> <weapon> <forward> <right> <up>
```

Angles are the encoded `usercmd_t` values (same as network snapshots).

### `*.betaevt` (high-level trace)

JSON Lines, one object per line:

```json
{"t":12345,"type":"map_loaded","source":"q3dm1","target":""}
```

Filter noisy traffic in mods; log only gameplay-significant messages (button pressed, door opened, level complete).

### `*.betatest` (manifest)

```ini
version=1
map=q3dm1
recorded_ms=45000
success=level_complete
fail=player_death
max_time_ms=120000
```

Semicolon-separated alternatives are supported on `success=` lines.

### `*.petrinet.json` (model, optional)

Describes places, transitions, and message bindings for **high-level** replay when maps change (paper §7). The runtime executor is not fully implemented yet; traces and Petri files are validated in CI and documented for mod authors.

## Workflow

### Regression after code changes

1. Human completes a level once: `beta_record my_run` → play → `beta_stop`.
2. After engine changes: `beta_play my_run` on the same map.
3. Compare console / event logs; mismatches indicate physics, input, or timing drift.

Use a fixed timestep (`com_timescale`, stable `com_maxfps`) for repeatable runs.

### Playability after map edits

1. Record with rich `beta_event` logging from the game module.
2. Maintain a Petri net JSON for the level puzzle (doors, switches, clones).
3. Future: `beta_test` will satisfy preconditions via navigation AI (paper §9); today use raw replay plus events.

### Mixed mode (paper §6)

If replay diverges on raw input, a future pass will switch to high-level action replay from the Petri net. Track with issue / roadmap in this doc.

## Demo mod example

See `examples/demo_game/beta_traces/README.md` and sample files `sample_level.*`.

## Dedicated / CI

Headless VMs cannot run the client; CI validates trace **file formats** via `tests/scripts/test_beta_trace_format.sh`. Full replay requires a display and game data.

## References

- Hernández Bécares, J., Costero Valero, L., Gómez Martín, P. P. (2017). *An approach to automated videogame beta testing.* Entertainment Computing, 18, 79–92.
- Related: `cl_demo.c` (network demo record), `com_timescale`, `docs/COMPATIBILITY.md`.
