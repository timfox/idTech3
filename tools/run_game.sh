#!/bin/bash

cd /home/tim/Desktop/idtech3/release

# Start the game in background
./idtech3.x86_64.so +set fs_game mymod +set r_mode 6 +set r_fullscreen 0 +set vid_xpos 100 +set vid_ypos 100 &
GAME_PID=$!

# Wait a moment for the window to appear
sleep 3

# Try to find and focus the window
if command -v wmctrl >/dev/null 2>&1; then
    echo "Trying to find and focus game window..."
    wmctrl -a "quake3" || wmctrl -a "idtech3" || wmctrl -a "ioquake3" || echo "Window not found with wmctrl"
elif command -v xdotool >/dev/null 2>&1; then
    echo "Trying to find and focus game window with xdotool..."
    xdotool search --name "quake3" windowactivate || xdotool search --name "idtech3" windowactivate || echo "Window not found with xdotool"
else
    echo "Neither wmctrl nor xdotool found. Install one of them to auto-focus the window."
    echo "Manual commands:"
    echo "  wmctrl -a quake3"
    echo "  or"
    echo "  xdotool search --name quake3 windowactivate"
fi

# Wait for game to exit
wait $GAME_PID