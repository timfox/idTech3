#!/bin/bash
# Test script to check library loading and .pk3dir filtering

echo "Testing library loading with debug output..."
echo "============================================"
echo ""

./release/idtech3.x86_64 +set fs_game mymod +set fs_debug 1 2>&1 | \
    grep -E "(FS_LoadLibrary|skipping|trying|dlopen|VM_LoadLib|UI module)" | \
    head -50

echo ""
echo "============================================"
echo "Test complete. Check output above for:"
echo "1. 'skipping .pk3dir directory' messages (should see these)"
echo "2. 'trying' messages for .pk3dir directories (should NOT see these)"
echo "3. 'VM_LoadLib.*ok' message (UI library should load successfully)"

