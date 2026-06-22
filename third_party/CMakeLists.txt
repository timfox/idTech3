# External targets helper
# Provides a minimal ext:: namespace for exporting external libraries as INTERFACE targets.
#
# Usage:
#   add_subdirectory(<external-lib>)   # if there is a subdir with a real library
#   include("${CMAKE_SOURCE_DIR}/external/CMakeLists.txt")
#   ADD_EXT_TARGET(nanosvg)
#
cmake_minimum_required(VERSION 3.16)
project(ExternalTargets LANGUAGES CXX)

# Registry of created ext targets (for potential later use)
set(_EXT_TARGETS "")

macro(ADD_EXT_TARGET NAME)
  set(_EXT_NAME "ext::${NAME}")
  if (NOT TARGET ${_EXT_NAME})
    add_library(${_EXT_NAME} INTERFACE)
  endif()
  # By default, do not attach include dirs or libs; downstreams can set them
  set(_EXT_TARGETS ${_EXT_TARGETS} ${_EXT_NAME} PARENT_SCOPE)
endmacro()

# Convenience macro to export a single library with include dirs and link libs
macro(EXPORT_EXT_LIBRARY NAME)
  set(_EXT_NAME "ext::${NAME}")
  if (NOT TARGET ${_EXT_NAME})
    add_library(${_EXT_NAME} INTERFACE)
  endif()
  # Arguments: INCLUDE_DIRS and LIBS are optional; caller can pass as CACHE or as args
  set(_INCLUDE_DIRS "")
  set(_LIBS "")
  if (DEFINED INCLUDE_DIRS)
    set(_INCLUDE_DIRS ${INCLUDE_DIRS})
  endif()
  if (DEFINED LIBS)
    set(_LIBS ${LIBS})
  endif()
  if (_INCLUDE_DIRS)
    target_include_directories(${_EXT_NAME} INTERFACE ${_INCLUDE_DIRS})
  endif()
  if (_LIBS)
    target_link_libraries(${_EXT_NAME} INTERFACE ${_LIBS})
  endif()
  set(_EXT_TARGETS ${_EXT_TARGETS} ${_EXT_NAME} PARENT_SCOPE)
endmacro()

# Example wrappers (can be pruned/expanded)
ADD_EXT_TARGET(nanosvg)       # wrapper for nanosvg, downstream can augment with include path
ADD_EXT_TARGET(vma)           # wrapper for Vulkan Memory Allocator (if needed)
ADD_EXT_TARGET(zstd)          # wrapper for zstd
ADD_EXT_TARGET(SDL2)           # wrapper for SDL2

# Optional: allow downstream to export actual external libs via EXPORT_EXT_LIBRARY(...)

