add_library(core_fsm
  Variable.cpp
  Transition.cpp
  State.cpp
  Automaton.cpp
)
target_include_directories(core_fsm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(fsm_demo
  main.cpp
)
target_link_libraries(fsm_demo PRIVATE core_fsm pthread)
