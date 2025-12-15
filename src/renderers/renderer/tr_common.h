// Compatibility shim for `src/renderers/renderercommon/*`.
//
// Those legacy sources include "../renderer/tr_common.h", but in this repo
// renderer-specific `tr_common.h` headers live under:
//   - src/renderers/opengl/tr_common.h
//   - src/renderers/opengl2/tr_common.h
//   - src/renderers/vulkan/tr_common.h
//   - src/renderers/d3d12/tr_common.h
//   - src/renderers/metal/tr_common.h
//
// Select the correct one based on active renderer compile definitions.

#pragma once

#if defined(USE_VULKAN) || defined(USE_VULKAN_API)
#include "../vulkan/tr_common.h"
#elif defined(USE_OPENGL2)
#include "../opengl2/tr_common.h"
#elif defined(USE_D3D12)
#include "../d3d12/tr_common.h"
#elif defined(USE_METAL)
#include "../metal/tr_common.h"
#else
#include "../opengl/tr_common.h"
#endif

