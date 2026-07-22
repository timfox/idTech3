# Clean-room Babble dialogue graphs (FAKK2/EF2 dialect notes)

See also [UBERTOOLS_CLEAN_ROOM.md](UBERTOOLS_CLEAN_ROOM.md).

## Enable

- Build: `USE_UBERTOOLS_COMPAT=ON` + `USE_BABBLE=ON` (default in `game`/`full`/`research`)
- Runtime: `g_babble 1`, `cl_subtitles 1`, `com_ubertools 1`

## File format (clean-room interchange)

```
# dialogue/intro.babble
graph intro
start greet
node greet
  speaker Alice
  loc dlg.intro.greet
  voice sound/voice/greet.wav
  duration 2.5
  next ask
node ask
  speaker Alice
  loc dlg.intro.ask
  choice Yes accept
  choice No decline
node accept
  speaker Alice
  loc dlg.intro.accept
node decline
  speaker Alice
  loc dlg.intro.decline
```

Display text is always resolved via `Com_Loc_Lookup` / `loc/<lang>.loc`.

## Console

- `babble_load <path>`
- `babble_start <graph>`
- `babble_advance [choiceIndex]`

## Lua

```lua
Engine.Babble.load("dialogue/intro.babble")
Engine.Babble.start("intro")
Engine.Babble.advance(0)
Engine.Babble.stop()
Engine.Dialogue.get(0)  -- {speaker,text,locKey,duration,choices}
Engine.Loc.lookup("dlg.intro.greet")
```

## Audio

Voice paths play through `S_StartLocalSound` and notify `SND_BUS_VOICE` ducking. **No Miles.**
