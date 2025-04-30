# fsm_runtime CMakeLists.txt
cmake_minimum_required(VERSION 3.15)              # or whatever your project uses
project(fsm_runtime)

# ---- Add this block so CMake can find the header-only JSON library ----
find_package(nlohmann_json CONFIG REQUIRED)       # or find_package(nlohmann_json 3.2.0 REQUIRED)
# -------------------------------------------------------------------------

add_executable(fsm_runtime runtime_main.cpp)

target_link_libraries(fsm_runtime
    PRIVATE
        core_fsm
        nlohmann_json::nlohmann_json
)

target_include_directories(fsm_runtime
    PRIVATE
        ${PROJECT_SOURCE_DIR}/../../src   # so it can find snapshot.hpp, core_fsm/, io_bridge/
)

set_target_properties(fsm_runtime PROPERTIES
    CXX_STANDARD     17
    CXX_STANDARD_REQUIRED YES
)
