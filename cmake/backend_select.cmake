# Basic cross-platform backend selection scaffolding.
#
# This file provides a minimal, non-breaking set of options to guide
# the renderer backend choice per platform. It is safe to modify/adapt
# as the project evolves.

if(WIN32)
  # On Windows, prefer Vulkan if available; users may override in cache.
  set(USE_VULKAN TRUE CACHE BOOL "Enable Vulkan backend" FORCE)
  set(USE_OPENGL FALSE CACHE BOOL "Disable OpenGL backend" FORCE)
elseif(APPLE)
  # macOS support: allow MoltenVK for Vulkan-on-M Metal or fall back to OpenGL/Metal path.
  option(USE_MOLTENVK "Enable MoltenVK backend on macOS" ON)
  if(USE_MOLTENVK)
    set(USE_VULKAN TRUE CACHE BOOL "Enable Vulkan backend (via MoltenVK on macOS)" FORCE)
  else()
    set(USE_VULKAN OFF CACHE BOOL "Disable Vulkan backend" FORCE)
  endif()
else()
  # Linux/Unix-like: default to Vulkan if the driver supports it.
  option(USE_VULKAN "Enable Vulkan backend" ON)
  set(USE_OPENGL OFF CACHE BOOL "Disable OpenGL backend" FORCE)
endif()

