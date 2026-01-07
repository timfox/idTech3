#!/bin/bash
# Interactive menu to choose which game to run

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== idTech3 Game Launcher ==="
echo "Available games:"
echo "1) OpenArena (OpenGL)"
echo "2) OpenArena (Vulkan)"
echo "3) Quake 3 Arena (base)"
echo "4) MyMod (custom game)"
echo "5) Exit"
echo ""

while true; do
    read -p "Choose a game (1-5): " choice
    case $choice in
        1)
            echo "Starting OpenArena (OpenGL)..."
            bash "$SCRIPT_DIR/run_openarena.sh"
            break
            ;;
        2)
            echo "Starting OpenArena (Vulkan)..."
            bash "$SCRIPT_DIR/run_openarena_vulkan.sh"
            break
            ;;
        3)
            echo "Starting Quake 3 Arena..."
            bash "$SCRIPT_DIR/run_quake3.sh"
            break
            ;;
        4)
            echo "Starting MyMod..."
            bash "$SCRIPT_DIR/run_mymod.sh"
            break
            ;;
        5)
            echo "Goodbye!"
            exit 0
            ;;
        *)
            echo "Invalid choice. Please enter 1-5."
            ;;
    esac
done