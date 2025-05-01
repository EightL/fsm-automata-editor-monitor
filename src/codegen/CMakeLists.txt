cmake_minimum_required(VERSION 3.10)
project(fsm_codegen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD     17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(fsm_codegen
  main.cpp
)

# core_fsm types & persistence, plus inja+json headers
target_include_directories(fsm_codegen PRIVATE
  "${CMAKE_SOURCE_DIR}/src/core_fsm"
  "${CMAKE_SOURCE_DIR}/external/inja"
  "${CMAKE_SOURCE_DIR}/external/nlohmann"
)

target_link_libraries(fsm_codegen
  PRIVATE core_fsm
          Threads::Threads
)
