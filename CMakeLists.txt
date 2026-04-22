cmake_minimum_required(VERSION 3.10)
project(icpc_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(code main.cpp)

# Enable optimizations for OJ
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(code PRIVATE -O2 -DNDEBUG)
else()
    target_compile_options(code PRIVATE -O0 -g)
endif()