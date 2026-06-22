# Shared include paths for moved extension trees.

function(idtech3_apply_research_include_dirs target)
  if(NOT IDTECH3_DIR_EXTENSIONS)
    include(${CMAKE_SOURCE_DIR}/cmake/IdTech3Layout.cmake)
  endif()
  target_include_directories(${target} PRIVATE
    ${IDTECH3_DIR_EXTENSIONS}/research
  )
endfunction()

function(idtech3_apply_generative_include_dirs target)
  if(NOT IDTECH3_DIR_EXTENSIONS)
    include(${CMAKE_SOURCE_DIR}/cmake/IdTech3Layout.cmake)
  endif()
  target_include_directories(${target} PRIVATE
    ${IDTECH3_DIR_EXTENSIONS}/generative
  )
endfunction()

macro(idtech3_apply_research_compile_defs target)
  if(USE_RESEARCH_EXTENSIONS)
    target_compile_definitions(${target} PRIVATE USE_RESEARCH_EXTENSIONS=1)
  endif()
  if(USE_OPEN_WORLD)
    target_compile_definitions(${target} PRIVATE USE_OPEN_WORLD=1)
  endif()
endmacro()
