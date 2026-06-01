# Demo game beta traces

Example assets for [docs/BETA_AUTOMATED_TESTING.md](../../../docs/BETA_AUTOMATED_TESTING.md).

| File | Role |
|------|------|
| `sample_level.betacmd` | Short synthetic usercmd trace |
| `sample_level.betaevt` | High-level JSONL events |
| `sample_level.betatest` | Test manifest (success / fail / timeout) |
| `time_space_door.petrinet.json` | Petri net for a switch-held door (paper Fig. 10 style) |

Copy recorded traces from `~/.idtech3/beta_traces/` into this directory for version control when they contain no secrets.

## Recording in-game

```
/set cl_betaTrace 1
beta_record sample_level
/map <your_map>
// play ...
beta_mark_success level_complete
beta_stop
```

## Replay

```
beta_play sample_level
beta_test sample_level
```
