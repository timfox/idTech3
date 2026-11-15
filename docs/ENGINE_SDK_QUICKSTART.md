# idTech3 Engine SDK

The idTech3 Engine SDK provides a conventional, project-oriented workflow: install the engine once, create self-contained game projects, edit with live reload, play in the same runtime, and package content as a `.pk3`. The tools retain native idTech3 formats and compatibility while presenting a straightforward create-to-ship workflow.

## Install

Build the engine, then install it for the current user:

```bash
./scripts/compile_engine.sh vulkan
./scripts/install_engine.sh
export PATH="$HOME/.local/bin:$PATH"
idtech3-engine doctor
```

Use `--prefix /opt/idtech3` for a system or redistributable layout. Packagers can combine it with `--destdir`. `--sdk-only` installs authoring tools without requiring a local engine build.

## Create, edit, play, ship

```bash
idtech3-engine new MyGame "$HOME/idtech3-projects/MyGame"
cd "$HOME/idtech3-projects/MyGame"
idtech3-engine editor .
idtech3-engine run .
idtech3-engine package .
```

`editor` starts the regular game runtime with the in-engine Studio panels enabled. Lua changes reload while the game runs. The generated `game.idproj` holds project identity and onboarding metadata, `mapsrc/` holds editable map sources, and the packaged `.pk3` is written by the existing validated asset pipeline. Licensed Quake III/OpenArena data is not bundled; standalone games must provide their own base data and content.

Run `idtech3-engine help` for all commands. Set `IDTECH3_ENGINE_DIR` to test against another engine build or `IDTECH3_SDK_ROOT` to select a different SDK tree.
