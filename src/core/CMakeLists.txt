# --- src/core/CMakeLists.txt ---

# Enable Qt's automatic code generation for this directory
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(core_fsm
    automaton.cpp
    state.cpp
    transition.cpp
    variable.cpp
    persistence_bridge.cpp
    script_engine.cpp          # uses QJSEngine
    io/udp_channel.cpp
    io/runtime_client.cpp      # <- owns signals/slots
)

target_include_directories(core_fsm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}          # your existing include
        ${EXTERNAL_DIR}                      # <— now "external/" is on the search path
)

target_link_libraries(core_fsm
    PUBLIC Threads::Threads
           Qt5::Core
           Qt5::Widgets          # drop if not really used here
    PUBLIC Qt5::Qml
)
