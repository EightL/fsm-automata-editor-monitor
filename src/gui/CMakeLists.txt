# -----------------------------------------------------------------------------
# @file   src/gui/CMakeLists.txt
# @brief  Build instructions for the FSM Editor GUI application and library.
#
# Enables Qt code-generation, adds source files for the GUI library,
# sets up include paths, and creates the executable application.
#
# @author Martin Ševčík (xsevcim00)
# @author Jakub Lůčný (xlucnyj00)
# @date   2025-05-06
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Enable Qt's automatic code generation
# -----------------------------------------------------------------------------
set(CMAKE_AUTOMOC   ON)  # Meta-Object Compiler for Qt signals/slots
set(CMAKE_AUTORCC   ON)  # Resource Compiler for .qrc files
set(CMAKE_AUTOUIC   ON)  # UI Compiler for .ui files
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}")  # Find .ui files

# -----------------------------------------------------------------------------
# GUI Qt client library
# -----------------------------------------------------------------------------
add_library(gui_qt_client
    # Main window components by functionality
    mainwindow/core.cpp          # Core functionality and initialization
    mainwindow/mainwindow.hpp    # Common header
    mainwindow/mainwindow.ui     # UI design
    mainwindow/creator.cpp       # Element creation
    mainwindow/editor.cpp        # Property editing
    mainwindow/file.cpp          # File operations
    mainwindow/runtime.cpp       # Runtime monitoring
    mainwindow/visualization.cpp # FSM visualization

    # Graphics components
    graphics/fsmgraphicsitems.cpp  # State/transition rendering
    graphics/fsmgraphicsitems.hpp
)

# Make the library headers accessible
target_include_directories(gui_qt_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against Qt modules and core FSM library
target_link_libraries(gui_qt_client
    PUBLIC Qt5::Core Qt5::Widgets Qt5::Qml
           core_fsm
)

# -----------------------------------------------------------------------------
# Main executable application
# -----------------------------------------------------------------------------
add_executable(gui_qt_client_exec
    gui_main.cpp
)

# Link against the GUI library
target_link_libraries(gui_qt_client_exec
    PRIVATE gui_qt_client
)

# Installation rules
install(TARGETS gui_qt_client_exec RUNTIME DESTINATION bin)