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

# ─── 1) codegen CLI ─────────────────────────────────────────────────────
add_subdirectory(src/codegen)

# ─── 2) core_fsm + GUI ──────────────────────────────────────────────────
add_subdirectory(src/core_fsm)

# ─── 3) runtime interpreter ─────────────────────────────────────────────
add_subdirectory(src/tools/fsm_runtime)

# ─── “generate” target for your example JSON ────────────────────────────
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated" CACHE STRING "")
set(FSM_JSON     "${CMAKE_SOURCE_DIR}/examples/TOF.fsm.json"
                 CACHE STRING "Path to input FSM JSON")

add_custom_target(generate
  COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
  COMMAND $<TARGET_FILE:fsm_codegen>
          "${FSM_JSON}"
          "${GENERATED_DIR}"
          "${CMAKE_SOURCE_DIR}/src/codegen/templates"
  DEPENDS fsm_codegen
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "🛠  Generating C++ sources from ${FSM_JSON}"
)

include_directories("${GENERATED_DIR}")
