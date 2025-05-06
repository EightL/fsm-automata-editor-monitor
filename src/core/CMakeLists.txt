# -----------------------------------------------------------------------------
# @file   src/core/CMakeLists.txt
# @brief  Build instructions for the core_fsm library (automaton, state machine,
#         persistence, scripting, and I/O channels).
#
# Enables Qt code-generation (MOC/UIC/RCC), pulls in source files, sets up
# include paths, and links against Threads and Qt modules.
#
# @author Martin Ševčík (xsevcim00)
# @author Jakub Lůčný (xlucnyj00)
# @date   2025-05-06
# -----------------------------------------------------------------------------

# Enable Qt’s automatic code generation (MOC/UIC/RCC) in this directory
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -----------------------------------------------------------------------------
# core_fsm library
# -----------------------------------------------------------------------------
add_library(core_fsm
    automaton.cpp
    state.cpp
    transition.cpp
    variable.cpp
    persistence_bridge.cpp
    script_engine.cpp          # uses QJSEngine for scripting support
    io/udp_channel.cpp         # low-level UDP transport
    io/runtime_client.cpp      # Qt-based client with signals/slots
)

# -----------------------------------------------------------------------------
# Include directories
# -----------------------------------------------------------------------------
target_include_directories(core_fsm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}  # expose core_fsm/*.hpp to dependents
        ${EXTERNAL_DIR}              # third-party/external headers
)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
target_link_libraries(core_fsm
    PUBLIC 
        Threads::Threads     # std::thread / pthreads
        Qt5::Core            # QObject, QTimer, QThread, etc.
        Qt5::Widgets         # drop if not actually used by core_fsm
        Qt5::Qml             # for script_engine or QJSEngine
)
