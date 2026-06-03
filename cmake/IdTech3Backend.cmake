# Optional idtech3backend submodule (https://github.com/timfox/idtech3backend)
# Not required for engine build. Include when USE_IDTECH3_BACKEND=ON and wiring game/server logic.

set(IDTECH3_BACKEND_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/src/external/idtech3backend")

if(USE_IDTECH3_BACKEND)
  if(NOT EXISTS "${IDTECH3_BACKEND_SUBMODULE_DIR}/README.md")
    message(FATAL_ERROR
      "USE_IDTECH3_BACKEND=ON but submodule missing at ${IDTECH3_BACKEND_SUBMODULE_DIR}\n"
      "  git submodule update --init src/external/idtech3backend")
  endif()
  message(STATUS "idtech3backend: submodule at ${IDTECH3_BACKEND_SUBMODULE_DIR}")
  # Future: add_subdirectory or target_link_libraries when backend ships CMake targets.
else()
  if(EXISTS "${IDTECH3_BACKEND_SUBMODULE_DIR}/README.md")
    message(STATUS "idtech3backend: submodule present (USE_IDTECH3_BACKEND=OFF)")
  endif()
endif()
