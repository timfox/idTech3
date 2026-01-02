#!/bin/bash

echo "=== Testing Default (X11) ==="
cd /home/tim/Desktop/idtech3
timeout 8s ./release/idtech3.x86_64 +fs_game mymod +set cl_renderer opengl +set r_fullscreen 0 +set r_windowed 1 +set r_width 800 +set r_height 600 +quit 2>&1 | strings | tail -10

echo ""
echo "=== Testing Wayland (+set r_wayland 1) ==="
timeout 8s ./release/idtech3.x86_64 +fs_game mymod +set cl_renderer opengl +set r_wayland 1 +set r_fullscreen 0 +set r_windowed 1 +set r_width 800 +set r_height 600 +quit 2>&1 | strings | tail -10

echo ""
echo "=== Testing SDL_VIDEODRIVER=x11 override ==="
SDL_VIDEODRIVER=x11 timeout 8s ./release/idtech3.x86_64 +fs_game mymod +set cl_renderer opengl +set r_fullscreen 0 +set r_windowed 1 +set r_width 800 +set r_height 600 +quit 2>&1 | strings | tail -10