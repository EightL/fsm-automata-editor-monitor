cmake_minimum_required(VERSION 3.15)
project(fsm_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/external")

# put everything in build/bin
foreach(dir RUNTIME LIBRARY ARCHIVE)
    set(CMAKE_${dir}_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()

# Qt5
list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt@5")
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Qml)

# pthread
find_package(Threads REQUIRED)

# external libs (header-only JSON)
find_package(nlohmann_json CONFIG REQUIRED)

# sub-projects
add_subdirectory(src/core)
add_subdirectory(src/gui)
add_subdirectory(src/fsm_runtime)
