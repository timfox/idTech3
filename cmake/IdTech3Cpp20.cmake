# IdTech3Cpp20.cmake — mixed C / C++20 migration options and flags.
# Included from top-level CMakeLists.txt after language standards are set.
#
# IMPORTANT: Do not apply -fno-exceptions / -fno-rtti globally — FetchContent
# third_party (e.g. FreeUSD) may require exceptions. First-party targets call
# idtech3_cpp20_apply_target_flags().

option(USE_CPP20 "Enable C++20 migration tooling, ABI guards, and converted leaf TUs" ON)
option(CPP20_EXCEPTIONS "Allow C++ exceptions in first-party engine C++ TUs" OFF)
option(CPP20_RTTI "Allow C++ RTTI in first-party engine C++ TUs" OFF)
option(CPP20_STRICT "Enable stricter warning set for first-party C++ translation units" ON)

message(STATUS "USE_CPP20=${USE_CPP20} CPP20_EXCEPTIONS=${CPP20_EXCEPTIONS} CPP20_RTTI=${CPP20_RTTI} CPP20_STRICT=${CPP20_STRICT}")

if(USE_CPP20)
	add_compile_definitions(USE_CPP20=1)
else()
	add_compile_definitions(USE_CPP20=0)
endif()

if(CPP20_EXCEPTIONS)
	add_compile_definitions(CPP20_EXCEPTIONS=1)
else()
	add_compile_definitions(CPP20_EXCEPTIONS=0)
endif()

if(CPP20_RTTI)
	add_compile_definitions(CPP20_RTTI=1)
else()
	add_compile_definitions(CPP20_RTTI=0)
endif()

# Ensure C++ standard stays at the detected IDTECH3_CXX_STANDARD (prefer 20).
if(USE_CPP20 AND IDTECH3_CXX_STANDARD LESS 20)
	message(WARNING "USE_CPP20=ON but toolchain C++ standard is ${IDTECH3_CXX_STANDARD}; prefer a C++20 compiler.")
endif()

# Apply no-exceptions / no-RTTI / strict warnings to a first-party target only.
function(idtech3_cpp20_apply_target_flags tgt)
	if(NOT TARGET ${tgt})
		return()
	endif()
	if(NOT CPP20_EXCEPTIONS)
		if(MSVC)
			target_compile_options(${tgt} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/EHs-c->)
		else()
			target_compile_options(${tgt} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
		endif()
	endif()
	if(NOT CPP20_RTTI)
		if(MSVC)
			target_compile_options(${tgt} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/GR->)
		else()
			target_compile_options(${tgt} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
		endif()
	endif()
	if(CPP20_STRICT AND NOT MSVC)
		target_compile_options(${tgt} PRIVATE
			$<$<COMPILE_LANGUAGE:CXX>:-Wextra>
			$<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
			$<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
			$<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
		)
	endif()
endfunction()
