#core fsm CMakeLists.txt
# 1) Find Qt5 Core + Widgets (for all of your GUI classes)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

# enable AUTOMOC/UIC so Q_OBJECT, ui_*.h and designer .ui files Just Work
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui_qt)

# 2) Pure FSM core library (unchanged)
add_library(core_fsm
  variable.cpp
  transition.cpp
  state.cpp
  automaton.cpp
  persistence_bridge.cpp
  io_bridge/udp_channel.cpp
)
target_include_directories(core_fsm 
  PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}      
    ${CMAKE_CURRENT_SOURCE_DIR}/io_bridge
    ${CMAKE_SOURCE_DIR}/external     
)
target_link_libraries(core_fsm
  PUBLIC pthread
)

# 3) Qt‐based UI client: mainwindow components as a library
add_library(ui_qt_client
  ui_qt/mainwindow.cpp
  ui_qt/mainwindow.hpp
  ui_qt/mainwindow.ui
)


# make sure the compiler sees your ui_qt directory
target_include_directories(ui_qt_client
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ui_qt
)

target_link_libraries(ui_qt_client
  PUBLIC
    Qt5::Core
    Qt5::Widgets
  PRIVATE
    core_fsm
)

# ----------------------------------------------------------------
# Our actual GUI executable
# ----------------------------------------------------------------
add_executable(ui_qt_client_exec
  ui_qt/gui_main.cpp          # New main entry point
  ui_qt/runtime_client.cpp    # Still needed for the RuntimeClient class
)

# Make sure it can see your ui/ headers
target_include_directories(ui_qt_client_exec
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui_qt
)

# Link against the library (which already drags in Qt & core_fsm)
target_link_libraries(ui_qt_client_exec
  PRIVATE ui_qt_client
)

# (optionally) install it
install(TARGETS ui_qt_client_exec RUNTIME DESTINATION bin)
