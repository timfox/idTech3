#!/usr/bin/env bash

# Allow building in a dedicated directory while keeping sources untouched.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_ROOT="$SCRIPT_DIR"
# default to a dedicated build/ directory so users no longer need to run
# `mkdir -p build && cd build` manually
BUILD_DIR="${BUILD_DIR:-$SRC_ROOT/build}"
EXTRA_FLAGS=("$@")

Q3MAP4_COMPILER_OPTIONS=(
  -MMD -O3 -march=native -fPIC -pipe -fno-strict-aliasing -fcommon
  -Wno-deprecated-declarations -Wno-format-overflow -Wno-stringop-overflow
  -I"$SRC_ROOT/include"
  -I"$SRC_ROOT/libs"
  -I/usr/include/libxml2
  -I/usr/include/glib-2.0
  -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
  -I/usr/include/libpng16
)

D_OPTIONS=(
  -DPOSIX -DXWINDOWS
  -DRADIANT_VERSION=\"1.5.0n\"
  -DRADIANT_MAJOR_VERSION=\"5\"
  -DRADIANT_MINOR_VERSION=\"0\"
  -DRADIANT_ABOUTMSG=\"Custom_build\"
  -DQ3MAP_VERSION=\"4.0.0n\"
  -DRADIANT_EXECUTABLE=\"x86_64\"
)

LIBS_COMPILER_OPTIONS=(
  -MMD -O3 -march=native -fPIC -pipe -fno-strict-aliasing -fcommon
  -Wno-deprecated-declarations -Wno-format-overflow -Wno-stringop-overflow
  -I"$SRC_ROOT/libs"
)

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

declare -a OBJECTS=()

compile_source() {
  local src_path="$1"
  local obj_path="${src_path%.c}.o"
  mkdir -p "$(dirname "$obj_path")"
  cc "${EXTRA_FLAGS[@]}" "${Q3MAP4_COMPILER_OPTIONS[@]}" "${D_OPTIONS[@]}" -c "$SRC_ROOT/$src_path" -o "$obj_path"
  OBJECTS+=("$obj_path")
}

compile_library() {
  local out_lib="$1"
  shift
  local objs=()
  mkdir -p "$(dirname "$out_lib")"
  for src in "$@"; do
    local obj="${src%.c}.o"
    mkdir -p "$(dirname "$obj")"
    cc "${EXTRA_FLAGS[@]}" "${LIBS_COMPILER_OPTIONS[@]}" "${D_OPTIONS[@]}" -c "$SRC_ROOT/$src" -o "$obj"
    objs+=("$obj")
  done
  ar rc "$out_lib" "${objs[@]}"
  ranlib "$out_lib"
}

# include/
for src in include/cmdlib.c include/imagelib.c include/inout.c include/jpeg.c include/md4.c \
           include/mutex.c include/polylib.c include/scriplib.c include/threads.c include/unzip.c include/vfs.c; do
  compile_source "$src"
done

# main/
for src in main/brush.c main/brush_primit.c main/bspfile_abstract.c main/bspfile_ibsp.c \
           main/bspfile_rbsp.c main/bsp.c main/convert_ase.c main/convert_obj.c main/convert_map.c \
           main/decals.c main/facebsp.c main/fog.c main/image.c main/leakfile.c main/light_bounce.c \
           main/lightmaps_ydnar.c main/light.c main/light_trace.c main/light_ydnar.c main/main.c \
           main/map.c main/mesh.c main/model.c main/patch.c main/path_init.c main/portals.c \
           main/prtfile.c main/shaders.c main/surface_extra.c main/surface_foliage.c main/surface_fur.c \
           main/surface_meta.c main/surface.c main/tjunction.c main/tree.c main/visflow.c main/vis.c \
           main/writebsp.c; do
  compile_source "$src"
done

# libs/ (shared object directories)
compile_library libs/ddslib.a libs/ddslib/ddslib.c
compile_library libs/filematch.a libs/filematch.c
compile_library libs/l_net.a libs/l_net/l_net.c libs/l_net/l_net_berkley.c
compile_library libs/mathlib.a libs/mathlib/bbox.c libs/mathlib/line.c libs/mathlib/m4x4.c libs/mathlib/mathlib.c libs/mathlib/ray.c
compile_library libs/picomodel.a \
  libs/picomodel/lwo/clip.c libs/picomodel/lwo/envelope.c libs/picomodel/lwo/list.c libs/picomodel/lwo/lwio.c \
  libs/picomodel/lwo/lwo2.c libs/picomodel/lwo/lwob.c libs/picomodel/lwo/pntspols.c libs/picomodel/lwo/surface.c \
  libs/picomodel/lwo/vecmath.c libs/picomodel/lwo/vmap.c libs/picomodel/picointernal.c libs/picomodel/picomodel.c \
  libs/picomodel/picomodules.c libs/picomodel/pm_3ds.c libs/picomodel/pm_ase.c libs/picomodel/pm_fm.c \
  libs/picomodel/pm_lwo.c libs/picomodel/pm_md2.c libs/picomodel/pm_md3.c libs/picomodel/pm_mdc.c \
  libs/picomodel/pm_ms3d.c libs/picomodel/pm_obj.c libs/picomodel/pm_terrain.c

g++ "$@" "${OBJECTS[@]}" libs/ddslib.a libs/filematch.a libs/l_net.a libs/mathlib.a libs/picomodel.a \
  -lxml2 -lglib-2.0 -lpng16 -lz -ljpeg -lpthread -o map-tool

du -b map-tool
file map-tool
echo "  removing *.o *.d *.a files"
rm -f include/*.o include/*.d libs/*.o libs/*.d libs/picomodel/*.o libs/picomodel/*.d \
      libs/picomodel/lwo/*.o libs/picomodel/lwo/*.d libs/ddslib/*.o libs/ddslib/*.d \
      libs/l_net/*.o libs/l_net/*.d libs/mathlib/*.o libs/mathlib/*.d main/*.o main/*.d *.a
