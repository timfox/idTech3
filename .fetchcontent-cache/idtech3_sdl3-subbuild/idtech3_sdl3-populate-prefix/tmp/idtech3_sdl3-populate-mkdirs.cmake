# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-src")
  file(MAKE_DIRECTORY "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-src")
endif()
file(MAKE_DIRECTORY
  "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-build"
  "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-subbuild/idtech3_sdl3-populate-prefix"
  "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-subbuild/idtech3_sdl3-populate-prefix/tmp"
  "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-subbuild/idtech3_sdl3-populate-prefix/src/idtech3_sdl3-populate-stamp"
  "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-subbuild/idtech3_sdl3-populate-prefix/src"
  "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-subbuild/idtech3_sdl3-populate-prefix/src/idtech3_sdl3-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-subbuild/idtech3_sdl3-populate-prefix/src/idtech3_sdl3-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/tim/Desktop/idtech3/.fetchcontent-cache/idtech3_sdl3-subbuild/idtech3_sdl3-populate-prefix/src/idtech3_sdl3-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
