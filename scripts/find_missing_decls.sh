#!/bin/bash

echo "Finding functions defined in Vulkan renderer but not declared in vk.h..."

# Get all function definitions from .c and .cpp files (excluding main headers)
find src/renderers/vulkan -name "*.c" -o -name "*.cpp" | xargs grep -h "^[a-zA-Z_][a-zA-Z0-9_]* [a-zA-Z_][a-zA-Z0-9_]*(" | grep -v "^static" | grep -v "^extern" | grep -v "^typedef" | grep -v "^#define" | sort -u > /tmp/func_defs.txt

# Get all function declarations from vk.h
grep "^[a-zA-Z_][a-zA-Z0-9_]* [a-zA-Z_][a-zA-Z0-9_]*(" src/renderers/vulkan/vk.h | grep -v "^extern" | grep -v "^typedef" | grep -v "^#define" | sort -u > /tmp/func_decls.txt

echo "Functions defined but not declared in vk.h:"
comm -23 /tmp/func_defs.txt /tmp/func_decls.txt | head -20

echo ""
echo "Total missing declarations:"
comm -23 /tmp/func_defs.txt /tmp/func_decls.txt | wc -l
