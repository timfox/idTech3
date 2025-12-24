#!/bin/bash

# Remove duplicate frame functions from vk.c
functions=(
    "vk_begin_frame"
    "vk_end_frame"
    "vk_present_frame"
    "vk_clear_color"
    "vk_clear_depth"
    "vk_read_pixels"
)

for func in "${functions[@]}"; do
    echo "Removing $func from vk.c..."
    # Find function start
    start_line=$(grep -n "void $func" src/renderers/vulkan/vk.c | cut -d: -f1)
    if [ -n "$start_line" ]; then
        # Find function end (next function or end of file)
        end_line=$(tail -n +$start_line src/renderers/vulkan/vk.c | grep -n "^void\|^qboolean\|^static" | head -2 | tail -1 | cut -d: -f1)
        if [ -z "$end_line" ]; then
            # Last function in file
            end_line=$(wc -l < src/renderers/vulkan/vk.c)
        else
            end_line=$((start_line + end_line - 2))
        fi
        
        echo "Removing lines $start_line to $end_line for $func"
        sed -i "${start_line},${end_line}d" src/renderers/vulkan/vk.c
    fi
done

echo "Done removing frame functions"
