# genout.cmake.in
# Generate .out from .c with awk (generic), based upon the automake logic.

# Copyright (c) 2022-2024 Cosmin Truta
# Copyright (c) 2016 Glenn Randers-Pehrson
# Written by Roger Leigh, 2016
#
# Use, modification and distribution are subject to
# the same licensing terms and conditions as libpng.
# Please see the copyright notice in png.h or visit
# http://libpng.org/pub/png/src/libpng-LICENSE.txt
#
# SPDX-License-Identifier: libpng-2.0

# Variables substituted from CMakeLists.txt
set(SRCDIR "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/libpng")
set(BINDIR "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/arm64-v8a/external/libpng")

set(AWK "/usr/bin/nawk")
set(CMAKE_C_COMPILER "/home/tim/android-sdk/ndk/27.0.12077973/toolchains/llvm/prebuilt/linux-x86_64/bin/clang")
set(CMAKE_C_FLAGS -g -DANDROID -fdata-sections -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -Wformat -Werror=format-security )
set(INCDIR "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/arm64-v8a/external/libpng")
set(PNG_PREFIX "")
set(PNGLIB_MAJOR "1")
set(PNGLIB_MINOR "6")
set(PNGLIB_VERSION "1.6.43")
set(ZLIBINCDIR "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/zlib")

set(PLATFORM_C_FLAGS)
if(APPLE)
  set(CMAKE_OSX_INTERNAL_ARCHITECTURES "")
  set(CMAKE_OSX_SYSROOT "")
  if(CMAKE_OSX_INTERNAL_ARCHITECTURES)
    set(PLATFORM_C_FLAGS ${PLATFORM_C_FLAGS} -arch ${CMAKE_OSX_INTERNAL_ARCHITECTURES})
  endif()
  if(CMAKE_OSX_SYSROOT)
    set(PLATFORM_C_FLAGS ${PLATFORM_C_FLAGS} -isysroot ${CMAKE_OSX_SYSROOT})
  endif()
endif()

get_filename_component(INPUTEXT "${INPUT}" EXT)
get_filename_component(OUTPUTEXT "${OUTPUT}" EXT)
get_filename_component(INPUTBASE "${INPUT}" NAME_WE)
get_filename_component(OUTPUTBASE "${OUTPUT}" NAME_WE)
get_filename_component(INPUTDIR "${INPUT}" PATH)
get_filename_component(OUTPUTDIR "${OUTPUT}" PATH)

if(INPUTEXT STREQUAL ".c" AND OUTPUTEXT STREQUAL ".out")
  get_filename_component(GENDIR "${OUTPUT}" PATH)
  file(MAKE_DIRECTORY "${GENDIR}")

  file(REMOVE "${OUTPUT}.tf1" "${OUTPUT}.tf2")

  set(INCLUDES "-I${INCDIR}")
  if(ZLIBINCDIR)
    foreach(dir ${ZLIBINCDIR})
      list(APPEND INCLUDES "-I${dir}")
    endforeach()
  endif()

  if(PNG_PREFIX)
    set(PNG_PREFIX_DEF "-DPNG_PREFIX=${PNG_PREFIX}")
  endif()

  execute_process(COMMAND "${CMAKE_C_COMPILER}" "-E"
                          ${CMAKE_C_FLAGS}
                          ${PLATFORM_C_FLAGS}
                          "-I${SRCDIR}"
                          "-I${BINDIR}"
                          ${INCLUDES}
                          "-DPNGLIB_LIBNAME=PNG${PNGLIB_MAJOR}${PNGLIB_MINOR}_0"
                          "-DPNGLIB_VERSION=${PNGLIB_VERSION}"
                          "-DSYMBOL_PREFIX=${SYMBOL_PREFIX}"
                          "-DPNG_NO_USE_READ_MACROS"
                          "-DPNG_BUILDING_SYMBOL_TABLE"
                          ${PNG_PREFIX_DEF}
                          "${INPUT}"
                  OUTPUT_FILE "${OUTPUT}.tf1"
                  WORKING_DIRECTORY "${BINDIR}"
                  RESULT_VARIABLE CPP_FAIL)
  if(CPP_FAIL)
    message(FATAL_ERROR "Failed to generate ${OUTPUT}.tf1")
  endif()

  execute_process(COMMAND "${AWK}" -f "${SRCDIR}/scripts/dfn.awk"
                          "out=${OUTPUT}.tf2" "${OUTPUT}.tf1"
                  WORKING_DIRECTORY "${BINDIR}"
                  RESULT_VARIABLE AWK_FAIL)
  if(AWK_FAIL)
    message(FATAL_ERROR "Failed to generate ${OUTPUT}.tf2")
  endif()

  file(REMOVE "${OUTPUT}.tf1")
  file(RENAME "${OUTPUT}.tf2" "${OUTPUT}")
else()
  message(FATAL_ERROR "Unsupported conversion: ${INPUTEXT} to ${OUTPUTEXT}")
endif()
