#core fsm CMakeLists.txt
# 1) Find Qt5 Core
find_package(Qt5 REQUIRED COMPONENTS Core)

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

# 3) Qt‐based RuntimeClient, now just link against Qt5::Core
add_library(ui_qt_client
  ui_qt/runtime_client.cpp
  ui_qt/runtime_client.hpp
)
target_link_libraries(ui_qt_client
  PRIVATE
    core_fsm
    Qt5::Core
)
