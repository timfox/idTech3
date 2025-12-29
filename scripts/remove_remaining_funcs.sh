#!/bin/bash

# Remove remaining duplicate functions from vk.c
functions=(
    "vk_begin_frame"
    "vk_end_frame" 
    "vk_present_frame"
    "vk_read_pixels"
)

for func in "${functions[@]}"; do
    echo "Removing $func from vk.c..."
    
    # Find function start
    start_line=$(grep -n "void $func" src/renderers/vulkan/vk.c | cut -d: -f1)
    
    if [ -n "$start_line" ]; then
        echo "Found $func at line $start_line"
        
        # Get function content and find end
        func_content=$(sed -n "${start_line},/^$/p" src/renderers/vulkan/vk.c | grep -v '^$')
        end_line=$((start_line + $(echo "$func_content" | wc -l) - 1))
        
        echo "Removing lines $start_line to $end_line for $func"
        sed -i "${start_line},${end_line}d" src/renderers/vulkan/vk.c
    else
        echo "Function $func not found"
    fi
done

echo "Done removing remaining functions"
