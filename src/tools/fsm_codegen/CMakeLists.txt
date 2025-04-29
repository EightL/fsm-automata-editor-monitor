cmake_minimum_required(VERSION 3.10)
project(fsm_codegen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
add_executable(fsm_codegen main.cpp)

# point at your core_fsm headers & persistence code:
target_include_directories(fsm_codegen PRIVATE
    ${CMAKE_SOURCE_DIR}/core_fsm
    ${CMAKE_SOURCE_DIR}/core_fsm/persistence
)

# link against nlohmann-json and any other core_fsm internals if needed:
target_link_libraries(fsm_codegen PRIVATE core_fsm)
