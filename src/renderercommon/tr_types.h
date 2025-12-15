// Compatibility shim for mods and legacy include paths.
//
// Some game code (e.g. mymod) expects renderercommon headers under:
//   src/renderercommon/
// but in this repository they live under:
//   src/renderers/renderercommon/
//
// Keep this header minimal and forward to the real definition.

#pragma once

#include "../renderers/renderercommon/tr_types.h"

