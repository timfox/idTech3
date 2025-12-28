if(APPLE)
  option(USE_MOLTENVK "Enable MoltenVK on macOS to run Vulkan on Metal" ON)
  if(USE_MOLTENVK)
    message(STATUS "MoltenVK path enabled on macOS")
    add_definitions(-DUSE_MOLTENVK)
  endif()
endif()

