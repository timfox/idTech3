cmake_minimum_required(VERSION 3.16)
project(idtech3_p2p_demo LANGUAGES CXX)

add_executable(p2p_demo p2p_demo.cpp)
target_compile_features(p2p_demo PRIVATE cxx_std_23)

if(MSVC)
	target_compile_definitions(p2p_demo PRIVATE _CRT_SECURE_NO_WARNINGS)
	target_link_libraries(p2p_demo PRIVATE ws2_32)
endif()

