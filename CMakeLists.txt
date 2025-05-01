# root CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(fsm_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Homebrew Qt5 path
list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt@5")

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_subdirectory(src/core_fsm)
# add_subdirectory(src/tools/fsm_codegen)
add_subdirectory(src/tools/fsm_runtime)
