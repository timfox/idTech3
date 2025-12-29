#!/bin/bash

# Functions to remove from vk.c because they exist in C++ files
functions=(
    "vk_draw_geometry"
    "vk_draw_dot" 
    "vk_begin_frame"
    "vk_end_frame"
    "vk_present_frame"
    "vk_clear_color"
    "vk_clear_depth"
    "vk_read_pixels"
    "vk_create_image"
    "vk_upload_image_data"
    "vk_create_blur_pipeline"
    "vk_update_post_process_pipelines"
)

for func in "${functions[@]}"; do
    echo "Removing $func from vk.c..."
    # Find the function and remove it
    sed -i "/void $func/,/^}/ { /void $func/ { h; d; }; H; /^}/ { x; /void $func/ d; x; d; }; d; }" src/renderers/vulkan/vk.c
done

echo "Done removing duplicate functions"
