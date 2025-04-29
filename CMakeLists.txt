cmake_minimum_required(VERSION 3.10)
project(fsm_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# this will descend into src/core_fsm/CMakeLists.txt
add_subdirectory(src/core_fsm)
