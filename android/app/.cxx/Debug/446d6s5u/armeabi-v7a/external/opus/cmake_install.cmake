# Install script for directory: /home/tim/Desktop/next-gen-4/idtech3/src/external/src/opus

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
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/armeabi-v7a/external/opus/libopus.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opus" TYPE FILE FILES
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/opus/include/opus.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/opus/include/opus_defines.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/opus/include/opus_multistream.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/opus/include/opus_projection.h"
    "/home/tim/Desktop/next-gen-4/idtech3/src/external/src/opus/include/opus_types.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/armeabi-v7a/external/opus/opus.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Opus/OpusTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Opus/OpusTargets.cmake"
         "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/armeabi-v7a/external/opus/CMakeFiles/Export/9d35469462eacf47198a472e89b71320/OpusTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Opus/OpusTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Opus/OpusTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Opus" TYPE FILE FILES "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/armeabi-v7a/external/opus/CMakeFiles/Export/9d35469462eacf47198a472e89b71320/OpusTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Opus" TYPE FILE FILES "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/armeabi-v7a/external/opus/CMakeFiles/Export/9d35469462eacf47198a472e89b71320/OpusTargets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Opus" TYPE FILE FILES
    "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/armeabi-v7a/external/opus/OpusConfig.cmake"
    "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/armeabi-v7a/external/opus/OpusConfigVersion.cmake"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/tim/Desktop/next-gen-4/idtech3/android/app/.cxx/Debug/446d6s5u/armeabi-v7a/external/opus/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
