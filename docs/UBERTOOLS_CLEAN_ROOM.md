# ÜberTools Clean-Room Compatibility

**License**: Engine remains **GPLv2**. This document records provenance and exclusion rules for FAKK2 / Elite Force II–era **Babble** and **TIKI** compatibility.

**Last Updated**: 2026-07-21

---

## What this is

Optional, toggle-gated loaders and runtimes that aim for **behavior-compatible** interchange with publicly described Ritual ÜberTools–era asset conventions (FAKK2 / EF2 dialect first). Implementation is **clean-room**: written from format notes and observed behavior, not from Ritual SDK source, Miles Sound System, or shipped game data.

## What this is not

- Not a port of the Ritual SDK, Babble tools, or TikiEd.
- Not Miles Sound System (proprietary). Audio remains **OpenAL** (`USE_OPENAL`) with SDL/null fallbacks.
- Not a license to redistribute retail FAKK2/EF2/MOHAA assets.
- Not a claim of full MOHAA dialect compatibility (extensible dialect IDs only).

## Gates

| Gate | Default (profile) | Role |
|------|-------------------|------|
| `USE_UBERTOOLS_COMPAT` | OFF in `core`; ON in `game`/`full`/`research` | Master chocolate-layer switch |
| `USE_BABBLE` | Follows master | Dialogue graph parser/runtime |
| `USE_TIKI` | Follows master | `.tik` / `.tan` parse + model composition |

Runtime cvars (startup-logged):

- `com_ubertools` — master runtime enable (0/1)
- `g_babble` — dialogue graphs
- `com_tiki` — TIKI model defs
- `cl_subtitles` — client subtitle presenter

## Provenance rules for contributors

1. Do **not** copy Ritual SDK headers, sources, or tools into this tree.
2. Do **not** add Miles (`mss.h`, `Miles`, `Bink`, proprietary middleware) headers, libs, or CMake options.
3. Synthetic fixtures under `tests/fixtures/ubertools/` must be original; never commit retail `.tik`/`.tan`/VO/meshes.
4. New files carry Gopex / GPL-compatible notices; document dialect limits in `docs/BABBLE.md` and `docs/TIKI.md`.
5. CI: `tests/scripts/test_ubertools_clean_room.sh` forbids Miles symbols and retail path markers.

## Architecture sketch

```
Babble (.babble / dialogue/) → Com_Loc_Lookup → EngineDialogue_* → cl_subtitles + OpenAL voice bus
TIKI (.tik) → mesh path → existing MOD_MD3/IQM/MDR/GLTF loaders + sidecar anim/events
Frame cmds → allowlisted Lua/particle/sound events (never raw Cbuf from assets)
```

## Validation

- Unit: `unit_babble_parse`, `unit_tiki_parse`, `unit_com_loc`
- Script: `test_ubertools_clean_room.sh`, profile matrix via existing `compile_engine.sh` profiles
- No retail assets in CI fixtures
