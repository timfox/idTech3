# Fetch and configure FreeUSD (GPL-2.0-or-later, https://github.com/gopexllc/FreeUSD)
# Engine uses C++ usdGeom mesh import + client scene inspection tools.
# Parent CMakeLists.txt must set USE_FREEUSD=ON before including this file.

include(FetchContent)

set(FREEUSD_BUILD_PYTHON OFF CACHE BOOL "FreeUSD: Python extension (off for engine embed)" FORCE)
set(FREEUSD_BUILD_TESTS OFF CACHE BOOL "FreeUSD: C++ tests (off for engine embed)" FORCE)
set(FREEUSD_BUILD_C_ABI ON CACHE BOOL "FreeUSD: C ABI library" FORCE)
set(FREEUSD_TEST_INSTALL_INTEGRATION OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  freeusd
  GIT_REPOSITORY https://github.com/gopexllc/FreeUSD.git
  GIT_TAG        ed71a11247a3e580c8f81d43342bf85e5e4610d2
  GIT_SHALLOW    TRUE
)

message(STATUS "FreeUSD: fetching from gopexllc/FreeUSD (USE_FREEUSD=ON)")

if(NOT TARGET freeusd::runtime)
  FetchContent_GetProperties(freeusd)
  if(NOT freeusd_POPULATED)
    FetchContent_Populate(freeusd)
    # Upstream install/CPack rules use CMAKE_SOURCE_DIR (top-level = idTech3 when embedded).
    # Rewrite to CMAKE_CURRENT_LIST_DIR so configure succeeds as a subproject.
    set(_freeusd_cmake "${freeusd_SOURCE_DIR}/CMakeLists.txt")
    if(EXISTS "${_freeusd_cmake}")
      file(READ "${_freeusd_cmake}" _freeusd_cmake_txt)
      string(REPLACE "\${CMAKE_SOURCE_DIR}/cmake/" "\${CMAKE_CURRENT_LIST_DIR}/cmake/" _freeusd_cmake_txt "${_freeusd_cmake_txt}")
      string(REPLACE "\${CMAKE_SOURCE_DIR}/LICENSE" "\${CMAKE_CURRENT_LIST_DIR}/LICENSE" _freeusd_cmake_txt "${_freeusd_cmake_txt}")
      string(REPLACE "\${CMAKE_SOURCE_DIR}/README.md" "\${CMAKE_CURRENT_LIST_DIR}/README.md" _freeusd_cmake_txt "${_freeusd_cmake_txt}")
      file(WRITE "${_freeusd_cmake}" "${_freeusd_cmake_txt}")
    endif()
  endif()
  add_subdirectory("${freeusd_SOURCE_DIR}" "${freeusd_BINARY_DIR}" EXCLUDE_FROM_ALL)
endif()

add_library(idtech3_freeusd INTERFACE)
target_link_libraries(idtech3_freeusd INTERFACE freeusd::runtime)
target_compile_definitions(idtech3_freeusd INTERFACE USE_FREEUSD=1)

message(STATUS "FreeUSD: linked freeusd::runtime (USDA-first; see docs/FREEUSD.md)")
