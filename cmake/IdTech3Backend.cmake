# Optional idtech3backend submodule (https://github.com/timfox/idtech3backend)
# Not required for engine build. Include when USE_IDTECH3_BACKEND=ON and wiring game/server logic.

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/idtech3backend/README.md")
  set(IDTECH3_BACKEND_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/third_party/idtech3backend")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/src/external/idtech3backend/README.md")
  set(IDTECH3_BACKEND_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/src/external/idtech3backend")
else()
  set(IDTECH3_BACKEND_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/third_party/idtech3backend")
endif()

set(IDTECH3_BACKEND_APP_CRDT_MANIFEST "${IDTECH3_BACKEND_SUBMODULE_DIR}/app_crdt/manifest.json")

if(EXISTS "${IDTECH3_BACKEND_APP_CRDT_MANIFEST}")
  set(IDTECH3_BACKEND_HAVE_APP_CRDT TRUE)
else()
  set(IDTECH3_BACKEND_HAVE_APP_CRDT FALSE)
endif()

function(idtech3_backend_apply_compile_defs target)
  if(IDTECH3_BACKEND_HAVE_APP_CRDT)
    target_compile_definitions(${target} PRIVATE
      "IDTECH3_BACKEND_DIR=\"${IDTECH3_BACKEND_SUBMODULE_DIR}\"")
  endif()
endfunction()

if(USE_IDTECH3_BACKEND)
  if(NOT EXISTS "${IDTECH3_BACKEND_SUBMODULE_DIR}/README.md")
    message(FATAL_ERROR
      "USE_IDTECH3_BACKEND=ON but submodule missing at ${IDTECH3_BACKEND_SUBMODULE_DIR}\n"
      "  git submodule update --init third_party/idtech3backend")
  endif()
  message(STATUS "idtech3backend: submodule at ${IDTECH3_BACKEND_SUBMODULE_DIR}")
  if(IDTECH3_BACKEND_HAVE_APP_CRDT)
    message(STATUS "idtech3backend: App CRDT manifest at app_crdt/manifest.json")
  endif()
else()
  if(EXISTS "${IDTECH3_BACKEND_SUBMODULE_DIR}/README.md")
    if(IDTECH3_BACKEND_HAVE_APP_CRDT)
      message(STATUS "idtech3backend: submodule present with App CRDT (USE_IDTECH3_BACKEND=OFF)")
    else()
      message(STATUS "idtech3backend: submodule present (USE_IDTECH3_BACKEND=OFF)")
    endif()
  endif()
endif()
