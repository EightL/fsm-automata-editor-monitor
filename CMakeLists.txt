cmake_minimum_required(VERSION 3.10)
project(fsm_toolkit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# only descend into src/ (and later tests/, examples/, etc.)
add_subdirectory(src)
