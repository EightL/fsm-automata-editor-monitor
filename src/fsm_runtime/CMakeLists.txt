add_executable(fsm_runtime
    runtime_main.cpp
)

# core_fsm include dirs already expose <io/runtime_client.hpp>
target_link_libraries(fsm_runtime
    PRIVATE core_fsm
            nlohmann_json::nlohmann_json
)

set_target_properties(fsm_runtime PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)
