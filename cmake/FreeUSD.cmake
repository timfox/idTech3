# FreeUSD (GPL-2.0-or-later, https://github.com/gopexllc/FreeUSD)
# Engine: USDA mesh import (renderer) + usd_* client tools.
# Parent CMakeLists.txt must set USE_FREEUSD=ON before including this file.
#
# Source order:
#   1. Git submodule  src/external/FreeUSD  (preferred; pin in parent commit)
#   2. FetchContent   when submodule is not initialized (offline-friendly fallback)

set(FREEUSD_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/src/external/FreeUSD")

set(FREEUSD_BUILD_PYTHON OFF CACHE BOOL "FreeUSD: Python extension (off for engine embed)" FORCE)
set(FREEUSD_BUILD_TESTS OFF CACHE BOOL "FreeUSD: C++ tests (off for engine embed)" FORCE)
set(FREEUSD_BUILD_C_ABI ON CACHE BOOL "FreeUSD: C ABI library" FORCE)
set(FREEUSD_TEST_INSTALL_INTEGRATION OFF CACHE BOOL "" FORCE)

function(_idtech3_freeusd_patch_embed_cmake freeusd_root)
  set(_freeusd_cmake "${freeusd_root}/CMakeLists.txt")
  if(NOT EXISTS "${_freeusd_cmake}")
    return()
  endif()
  file(READ "${_freeusd_cmake}" _freeusd_cmake_txt)
  string(REPLACE "\${CMAKE_SOURCE_DIR}/cmake/" "\${CMAKE_CURRENT_LIST_DIR}/cmake/" _freeusd_cmake_txt "${_freeusd_cmake_txt}")
  string(REPLACE "\${CMAKE_SOURCE_DIR}/LICENSE" "\${CMAKE_CURRENT_LIST_DIR}/LICENSE" _freeusd_cmake_txt "${_freeusd_cmake_txt}")
  string(REPLACE "\${CMAKE_SOURCE_DIR}/README.md" "\${CMAKE_CURRENT_LIST_DIR}/README.md" _freeusd_cmake_txt "${_freeusd_cmake_txt}")
  file(WRITE "${_freeusd_cmake}" "${_freeusd_cmake_txt}")
endfunction()

function(_idtech3_freeusd_add_from_source freeusd_root)
  if(NOT TARGET freeusd::runtime)
    add_subdirectory("${freeusd_root}" "${CMAKE_BINARY_DIR}/_deps/freeusd-build" EXCLUDE_FROM_ALL)
  endif()
endfunction()

function(_idtech3_freeusd_stage_submodule_copy out_root)
  set(_src "${FREEUSD_SUBMODULE_DIR}")
  set(_dst "${CMAKE_BINARY_DIR}/_deps/freeusd-src")
  set(_stamp "${_dst}/.idtech3_freeusd_copy_stamp")
  if(NOT EXISTS "${_src}/CMakeLists.txt")
    message(FATAL_ERROR "FreeUSD submodule missing: ${_src} (run: git submodule update --init src/external/FreeUSD)")
  endif()
  if(NOT EXISTS "${_stamp}" OR "${_src}/CMakeLists.txt" IS_NEWER_THAN "${_stamp}")
    file(REMOVE_RECURSE "${_dst}")
    file(COPY "${_src}/" DESTINATION "${_dst}")
    file(TOUCH "${_stamp}")
    message(STATUS "FreeUSD: staged submodule copy to ${_dst}")
  endif()
  _idtech3_freeusd_patch_embed_cmake("${_dst}")
  set("${out_root}" "${_dst}" PARENT_SCOPE)
endfunction()

if(NOT TARGET freeusd::runtime)
  if(EXISTS "${FREEUSD_SUBMODULE_DIR}/CMakeLists.txt")
    message(STATUS "FreeUSD: using Git submodule at src/external/FreeUSD")
    _idtech3_freeusd_stage_submodule_copy(_freeusd_root)
    _idtech3_freeusd_add_from_source("${_freeusd_root}")
  else()
    include(FetchContent)
    FetchContent_Declare(
      freeusd
      GIT_REPOSITORY https://github.com/gopexllc/FreeUSD.git
      GIT_TAG        ed71a11247a3e580c8f81d43342bf85e5e4610d2
      GIT_SHALLOW    TRUE
    )
    message(STATUS "FreeUSD: submodule missing; FetchContent from gopexllc/FreeUSD")
    message(STATUS "FreeUSD: run: git submodule update --init src/external/FreeUSD")
    FetchContent_GetProperties(freeusd)
    if(NOT freeusd_POPULATED)
      FetchContent_Populate(freeusd)
    endif()
    _idtech3_freeusd_patch_embed_cmake("${freeusd_SOURCE_DIR}")
    _idtech3_freeusd_add_from_source("${freeusd_SOURCE_DIR}")
  endif()
endif()

add_library(idtech3_freeusd INTERFACE)
target_link_libraries(idtech3_freeusd INTERFACE freeusd::runtime freeusd::c)
target_compile_definitions(idtech3_freeusd INTERFACE USE_FREEUSD=1)

message(STATUS "FreeUSD: linked freeusd::runtime + freeusd::c (see docs/FREEUSD.md)")
