#!/usr/bin/env python3
import os, sys, zipfile

def main():
    base_dir = "/home/tim/Desktop/idtech3/base"
    mods_demo = "/home/tim/Desktop/idtech3/mods/demo"
    if not os.path.isdir(mods_demo):
        print("Mods/demo not found. Aborting.", flush=True)
        return 1
    os.makedirs(base_dir, exist_ok=True)
    pak_path = os.path.join(base_dir, "pak0.pk3")
    # Allow overriding source of content by env var or CLI arg
    external_pak = None
    if len(sys.argv) > 1:
        external_pak = sys.argv[1]
    # If external pak provided, copy it directly
    if external_pak and os.path.exists(external_pak):
        import shutil
        shutil.copy2(external_pak, pak_path)
        print("Copied external pak0.pk3 from", external_pak, "to", pak_path)
        return 0

    # Build a clean PK3 from demo content
    with zipfile.ZipFile(pak_path, "w", zipfile.ZIP_DEFLATED) as z:
        # Add map
        src_maps = os.path.join(mods_demo, "maps", "demo.bsp")
        if not (os.path.exists(src_maps) and os.path.getsize(src_maps) > 0):
            print("Error: maps/demo.bsp missing or empty; cannot create valid pak0.pk3.", flush=True)
            return 1
        z.write(src_maps, arcname="maps/demo.bsp")
        # Textures
        for src, arc in [
            (os.path.join(mods_demo, "textures", "demo", "floor.tga"), "textures/demo/floor.tga"),
            (os.path.join(mods_demo, "textures", "demo", "wall.tga"), "textures/demo/wall.tga"),
        ]:
            if os.path.exists(src) and os.path.getsize(src) > 0:
                z.write(src, arcname=arc)
            else:
                print("Warning: texture missing or empty, creating placeholder:", arc, flush=True)
                z.writestr(arc, "")
    print("Created pak0.pk3 at", pak_path)
    return 0

if __name__ == "__main__":
    sys.exit(main())
