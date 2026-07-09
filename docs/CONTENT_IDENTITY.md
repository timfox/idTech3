# Content And Identity

An engine becomes easier to adopt when it ships a clear default game shell, not just rendering features and extension docs.

This repository now has three layers of starter content:

## 1. Bootstrap shell

For the smallest valid filesystem layout, use [MINIMAL_GAME_SHELL.md](MINIMAL_GAME_SHELL.md).

That layer answers: “How do I make the engine boot?”

## 2. Demo shell

For a ready-to-run engine-authored showcase, use:

- [examples/demo_game/README.md](../examples/demo_game/README.md)
- [examples/demo_skeleton/README.md](../examples/demo_skeleton/README.md)

That layer answers: “What does this engine look like when it is already wired up?”

## 3. Starter project

For a new game with opinionated workflows, use:

```bash
./scripts/create_starter_game.sh mygame ./release/mygame
```

That produces a project with:

- branded `gameinfo.txt`
- `autoexec.cfg`
- Lua hot-reload shell
- localization table
- `game.idproj`
- `run_dev.sh`
- `pack_game.sh`
- `asset_pipeline.conf`

That layer answers: “How should I start building my own game here?”

## Recommended first-hour workflow

1. Build the engine: `./scripts/compile_engine.sh vulkan`
2. Create a starter game: `./scripts/create_starter_game.sh mygame ./release/mygame`
3. Generate a package: `cd ./release/mygame && ./pack_game.sh --skip-shaders`
4. Launch the shell: `./run_dev.sh`
5. Edit `scripts/lua/main.lua`
6. In the console, try:

```text
starter_status
lua_run starter_boot()
lua_run starter_checkpoint()
```

## Why this matters

The goal is to make the engine feel like it has a default product posture:

- **content**: there is sample structure, starter copy, and a recognizable shell
- **identity**: there is a recommended way to name, launch, hot reload, and package a game
- **workflow**: there is a short path from scaffold to playable prototype

Keep using `game.minimal` when you need a tiny low-level template. Use `game.starter` when you want the default “build something fun quickly” path.
