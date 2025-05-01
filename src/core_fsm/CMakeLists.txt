cmake_minimum_required(VERSION 3.10)
project(core_fsm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD     17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
set(CMAKE_AUTOMOC   ON)
set(CMAKE_AUTOUIC   ON)
set(CMAKE_AUTORCC   ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui_qt")

# ——— library of your FSM core —————————————————————————————————————
add_library(core_fsm
  variable.cpp
  transition.cpp
  state.cpp
  automaton.cpp
  persistence_bridge.cpp
  io_bridge/udp_channel.cpp
)

target_include_directories(core_fsm PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/io_bridge"
  "${CMAKE_SOURCE_DIR}/external"
)

target_link_libraries(core_fsm PUBLIC Threads::Threads)

# ——— Qt GUI code as a lib ————————————————————————————————————————
add_library(ui_qt_client
  ui_qt/mainwindow.cpp
  ui_qt/mainwindow.hpp
  ui_qt/mainwindow.ui
)

target_include_directories(ui_qt_client PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/ui_qt"
)

target_link_libraries(ui_qt_client
  PUBLIC Qt5::Core Qt5::Widgets
  PRIVATE core_fsm
)

# — propagate codegen info into every TU of the GUI lib ————————
target_compile_definitions(ui_qt_client
  PUBLIC
    CODEGEN_EXE="$<TARGET_FILE:fsm_codegen>"
    GENERATED_DIR="${CMAKE_BINARY_DIR}/generated"
    SRC_TEMPLATE_DIR="${CMAKE_SOURCE_DIR}/src/codegen/templates"
)

# ——— final GUI executable ————————————————————————————————————————
add_executable(ui_qt_client_exec
  ui_qt/gui_main.cpp
  ui_qt/runtime_client.cpp
)

target_link_libraries(ui_qt_client_exec PRIVATE ui_qt_client)

# (optional) still define for the exe if you need them PRIVATE too
target_compile_definitions(ui_qt_client_exec PRIVATE
  CODEGEN_EXE="$<TARGET_FILE:fsm_codegen>"
  GENERATED_DIR="${CMAKE_BINARY_DIR}/generated"
)

install(TARGETS ui_qt_client_exec RUNTIME DESTINATION bin)
