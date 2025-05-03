cmake_minimum_required(VERSION 3.10)
project(fsm_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD     17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── put ALL executables into build/bin ──────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ─── Qt + pthread globally ───────────────────────────────────────────────
list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt@5")
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Threads REQUIRED)   # for pthread

# ─── 2) core_fsm + GUI ──────────────────────────────────────────────────
add_subdirectory(src/core_fsm)

# ─── 3) runtime interpreter ─────────────────────────────────────────────
add_subdirectory(src/tools/fsm_runtime)
