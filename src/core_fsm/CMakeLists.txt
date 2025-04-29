add_library(core_fsm
  variable.cpp
  transition.cpp
  state.cpp
  automaton.cpp
  persistence_bridge.cpp
)
target_include_directories(core_fsm 
  PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/external 
)

add_executable(fsm_demo
  main.cpp
)
target_link_libraries(fsm_demo PRIVATE core_fsm pthread)
