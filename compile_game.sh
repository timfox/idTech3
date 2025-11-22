#!/bin/bash

# Compile Game Script for id Tech 3 mod

set -e

# Navigate to the game mod build scripts directory
cd mymod/scripts/build

# Run the build process
make

echo "Game mod build completed. Check the output in mymod/scripts/build."
