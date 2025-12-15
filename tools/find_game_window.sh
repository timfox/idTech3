#!/bin/bash

echo "🔍 Looking for the id Tech 3 game window..."
echo ""

# Check if the game is running
if ! pgrep -f "idtech3.x86_64.so" >/dev/null; then
    echo "❌ Game is not running. Please start it first with:"
    echo "   cd /home/tim/Desktop/idtech3/release"
    echo "   ./idtech3.x86_64.so +set fs_game mymod +set r_mode 6 +set r_fullscreen 0"
    exit 1
fi

echo "✅ Game is running"

# Look for the game window
GAME_WINDOW=$(xwininfo -root -children 2>/dev/null | grep -i "id tech 3" | head -1)

if [ -z "$GAME_WINDOW" ]; then
    echo "❌ Could not find 'id Tech 3' window"
    echo ""
    echo "📋 All current windows:"
    xwininfo -root -children 2>/dev/null | grep -E "0x[0-9a-f]+" | head -10
else
    echo "✅ Found game window:"
    echo "$GAME_WINDOW"
    echo ""

    # Try to focus the window using different methods
    WINDOW_ID=$(echo "$GAME_WINDOW" | awk '{print $1}')

    echo "🎯 Attempting to focus the window..."

    # Method 1: wmctrl (if available)
    if command -v wmctrl >/dev/null 2>&1; then
        echo "Using wmctrl..."
        wmctrl -i -a "$WINDOW_ID" 2>/dev/null && echo "✅ Success with wmctrl" && exit 0
    fi

    # Method 2: xdotool (if available)
    if command -v xdotool >/dev/null 2>&1; then
        echo "Using xdotool..."
        xdotool windowactivate "$WINDOW_ID" 2>/dev/null && echo "✅ Success with xdotool" && exit 0
    fi

    # Method 3: Manual instructions
    echo "❌ Could not auto-focus window (wmctrl/xdotool not available)"
    echo ""
    echo "📋 Manual steps to find the window:"
    echo "1. Press Alt+Tab to cycle through windows"
    echo "2. Check other workspaces/virtual desktops"
    echo "3. Look for a window titled 'id Tech 3'"
    echo "4. The window should be approximately 1024x768 pixels"
    echo ""
    echo "🛠️  To install window management tools for auto-focus:"
    echo "   sudo apt install wmctrl"
    echo "   OR"
    echo "   sudo apt install xdotool"
fi