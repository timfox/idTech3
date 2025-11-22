#!/bin/bash

# Run Game Script for id Tech 3 with mymod, with logging

set -e

GAME_DIR="/home/tim/Desktop/idtech3/build"
GAME_EXEC="./idtech3.x86_64"
# Start with a map if one is available, otherwise just start the game
# You can add +map q3dm1 or any other map name to start directly in a map
GAME_ARGS="+set fs_game mymod +set vm_game 0 +set vm_cgame 0 +set vm_ui 0"

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

