#!/bin/bash

# Run Game Script for id Tech 3 with mymod, with logging

set -e

# Get the script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build"

GAME_DIR="$BUILD_DIR"
GAME_EXEC="./idtech3.x86_64"
# Start with a map if one is available, otherwise just start the game
# You can add +map q3dm1 or any other map name to start directly in a map
# fs_basepath defaults to the directory containing the executable (build/)
# So mymod files should be at build/mymod/
GAME_ARGS="+set fs_basepath $BUILD_DIR +set fs_game mymod +set vm_game 0 +set vm_cgame 0 +set vm_ui 0"

echo "--------------------------------------------"
echo "Launching id Tech 3 with the following setup:"
echo "  Directory : $GAME_DIR"
echo "  Executable: $GAME_EXEC"
echo "  Arguments : $GAME_ARGS"
echo "--------------------------------------------"

cd "$GAME_DIR"
echo "Changed directory to $(pwd)"

if [ ! -x "$GAME_EXEC" ]; then
    echo "Error: Game executable '$GAME_EXEC' not found or not executable!"
    exit 1
fi

echo "Running the game..."
$GAME_EXEC $GAME_ARGS

echo "Game exited with code $?"

