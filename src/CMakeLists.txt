# find Qt here
find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_executable(fsm_gui main.cpp)
target_link_libraries(fsm_gui Qt5::Widgets)
