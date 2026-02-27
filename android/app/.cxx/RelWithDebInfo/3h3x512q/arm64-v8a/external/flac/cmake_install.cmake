# Install script for directory: /home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/tim/android-sdk/ndk/27.0.12077973/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/FLAC" TYPE FILE FILES
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/all.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/assert.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/callback.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/export.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/format.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/metadata.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/ordinals.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/stream_decoder.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC/stream_encoder.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/FLAC++" TYPE FILE FILES
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC++/all.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC++/decoder.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC++/encoder.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC++/export.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/flac/include/FLAC++/metadata.h"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/RelWithDebInfo/3h3x512q/arm64-v8a/external/flac/src/cmake_install.cmake")
  include("/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/RelWithDebInfo/3h3x512q/arm64-v8a/external/flac/microbench/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/RelWithDebInfo/3h3x512q/arm64-v8a/external/flac/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
