# Clean-room TIKI / TAN (FAKK2 / Elite Force II dialect)

See also [UBERTOOLS_CLEAN_ROOM.md](UBERTOOLS_CLEAN_ROOM.md).

## Enable

- Build: `USE_TIKI=ON` (with `USE_UBERTOOLS_COMPAT`)
- Runtime: `com_tiki 1`

## `.tik` text (subset)

```
setup
{
  path models/players/demo/body.md3
  scale 1.0
  lod models/players/demo/body_1.md3 500
  surface head models/players/demo/head
}

animations
{
  idle models/players/demo/idle.tan
  walk {
    path models/players/demo/walk.tan
    framerate 20
    cmd 5 footstep sound/player/step.wav
  }
}
```

Frame commands are **allowlisted** (`sound`, `effect`, `footstep`, `particle`, `dialogue`, …). Arbitrary console text from assets is rejected.

## Registration

`RE_RegisterModel("models/foo.tik")` parses the def and composes into existing MD3/IQM/MDR/GLTF loaders. Animation aliases live in a sidecar keyed by `qhandle_t` (`R_Tiki_GetSidecar`).

## `.tan`

Little-endian header decode only (`TAN ` + version/frames/bones). Full skeletal playback maps through existing IQM/MD3 animation paths when mesh is those formats.
