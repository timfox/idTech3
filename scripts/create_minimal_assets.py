#!/usr/bin/env python3
"""
Create minimal texture assets for OpenArena compatibility
"""
import struct
import os

def create_tga_header(width, height):
    """Create a minimal TGA header"""
    # TGA header for 32-bit RGBA uncompressed
    return struct.pack('<BBBBBBBBBBBBHHBB',
        0,  # ID length
        0,  # Color map type
        2,  # Image type (uncompressed RGB)
        0, 0, 0, 0, 0,  # Color map spec (not used)
        0, 0,  # X origin
        0, 0,  # Y origin
        width, height,  # Width, height
        32,  # Bits per pixel
        8   # Image descriptor
    )

def create_solid_color_tga(width, height, r, g, b, a=255):
    """Create a solid color TGA file"""
    header = create_tga_header(width, height)
    # Create pixel data (BGRA format for TGA)
    pixel_data = bytes([b, g, r, a] * (width * height))
    return header + pixel_data

def save_tga(filename, data):
    """Save TGA data to file"""
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with open(filename, 'wb') as f:
        f.write(data)
    print(f"Created: {filename}")

# Create necessary directories
os.makedirs('baseq3/gfx/2d', exist_ok=True)
os.makedirs('baseq3/console', exist_ok=True)
os.makedirs('baseq3/textures/sfx', exist_ok=True)

# Create minimal texture assets
assets = [
    ('baseq3/gfx/2d/bigchars.tga', 256, 128, 255, 255, 255),  # White font texture
    ('baseq3/console/background.tga', 64, 64, 0, 0, 0),       # Black console background
    ('baseq3/textures/sfx/logo256.tga', 256, 256, 255, 0, 0), # Red logo placeholder
    ('baseq3/textures/sfx/console01.tga', 128, 128, 64, 64, 64), # Gray console texture
]

for filename, width, height, r, g, b in assets:
    tga_data = create_solid_color_tga(width, height, r, g, b)
    save_tga(filename, tga_data)

print("Minimal OpenArena assets created successfully!")
