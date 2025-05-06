# -----------------------------------------------------------------------------
# @file   src/fsm_runtime/CMakeLists.txt
# @brief  Build instructions for the fsm_runtime executable (standalone FSM 
#         runtime environment for executing state machines).
#
# Sets up the executable target, links against required libraries,
# and configures C++ standard requirements.
#
# @author Martin Ševčík (xsevcim00)
# @author Jakub Lůčný (xlucnyj00)
# @date   2025-05-06
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# fsm_runtime executable
# -----------------------------------------------------------------------------
add_executable(fsm_runtime
    runtime_main.cpp
)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
# core_fsm include dirs already expose <io/runtime_client.hpp>
target_link_libraries(fsm_runtime
    PRIVATE core_fsm              # FSM core functionality
            nlohmann_json::nlohmann_json  # JSON parsing/serialization
)

# -----------------------------------------------------------------------------
# Build configuration
# -----------------------------------------------------------------------------
set_target_properties(fsm_runtime PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)