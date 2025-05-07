# -----------------------------------------------------------------------------
# @file   CMakeLists.txt
# @brief  Top-level CMake configuration for the FSM editor project.
#
# Sets up basic project requirements, finds dependencies, and includes
# three main components: core FSM library, GUI editor, and runtime interpreter.
#
# @author Martin Ševčík (xsevcim00)
# @author Jakub Lůčný (xlucnyj00)
# @date   2025-05-06
# -----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.15)
project(fsm_demo LANGUAGES CXX)

#
# ————————————————————————————————————————————————
# header-only JSON library living in external/nlohmann/json.hpp
# ————————————————————————————————————————————————
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json
  INTERFACE
    ${CMAKE_SOURCE_DIR}/external/nlohmann
)
# create the standard CMake alias so subdirs can do
#   target_link_libraries(... nlohmann_json::nlohmann_json)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

# Set C++17 as required standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For tools like clang-tidy

set(EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/external")

# Configure all output binaries to a single location for easier testing
foreach(dir RUNTIME LIBRARY ARCHIVE)
    set(CMAKE_${dir}_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()

# Find Qt5 dependencies - core functionality, UI widgets, and QML
list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt@5")
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Qml)

# Find threading library for concurrent operations
find_package(Threads REQUIRED)

# Include the three main components of the project
add_subdirectory(src/core)        # FSM core functionality library
add_subdirectory(src/gui)         # Qt-based graphical editor
add_subdirectory(src/fsm_runtime) # State machine interpreter