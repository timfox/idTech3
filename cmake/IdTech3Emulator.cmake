# Optional idTech3-Emulator submodule (https://github.com/timfox/idTech3-Emulator)
# QEMU fork for in-game OS sandboxing. Not required for engine build; not linked by default.
# Engine-side display bridge (Vulkan render texture) is planned under USE_IDTECH3_EMULATOR.

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/idtech3-emulator/README.rst")
  set(IDTECH3_EMULATOR_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/third_party/idtech3-emulator")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/src/external/idtech3-emulator/README.rst")
  set(IDTECH3_EMULATOR_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/src/external/idtech3-emulator")
else()
  set(IDTECH3_EMULATOR_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/third_party/idtech3-emulator")
endif()

set(IDTECH3_EMULATOR_QEMU_SYSTEM "${IDTECH3_EMULATOR_SUBMODULE_DIR}/build/qemu-system-x86_64")

function(idtech3_emulator_apply_compile_defs target)
  if(EXISTS "${IDTECH3_EMULATOR_SUBMODULE_DIR}/README.rst")
    target_compile_definitions(${target} PRIVATE
      "IDTECH3_EMULATOR_DIR=\"${IDTECH3_EMULATOR_SUBMODULE_DIR}\"")
  endif()
endfunction()

if(USE_IDTECH3_EMULATOR)
  if(NOT EXISTS "${IDTECH3_EMULATOR_SUBMODULE_DIR}/README.rst")
    message(FATAL_ERROR
      "USE_IDTECH3_EMULATOR=ON but submodule missing at ${IDTECH3_EMULATOR_SUBMODULE_DIR}\n"
      "  git submodule update --init third_party/idtech3-emulator")
  endif()
  message(STATUS "idTech3-Emulator: submodule at ${IDTECH3_EMULATOR_SUBMODULE_DIR}")
  if(EXISTS "${IDTECH3_EMULATOR_QEMU_SYSTEM}")
    message(STATUS "idTech3-Emulator: built qemu-system-x86_64 at ${IDTECH3_EMULATOR_QEMU_SYSTEM}")
  else()
    message(STATUS "idTech3-Emulator: build QEMU in submodule (see docs/IDTECH3_EMULATOR.md)")
  endif()
else()
  if(EXISTS "${IDTECH3_EMULATOR_SUBMODULE_DIR}/README.rst")
    message(STATUS "idTech3-Emulator: submodule present (USE_IDTECH3_EMULATOR=OFF)")
  endif()
endif()
