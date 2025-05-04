set(CMAKE_AUTOMOC   ON)
set(CMAKE_AUTORCC   ON)
set(CMAKE_AUTOUIC   ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(gui_qt_client
    mainwindow/core.cpp
    mainwindow/mainwindow.hpp
    mainwindow/mainwindow.ui
    mainwindow/creator.cpp
    mainwindow/editor.cpp
    mainwindow/file.cpp
    mainwindow/runtime.cpp
    mainwindow/visualization.cpp

    graphics/fsmgraphicsitems.cpp
    graphics/fsmgraphicsitems.hpp
)

target_include_directories(gui_qt_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gui_qt_client
    PUBLIC Qt5::Core Qt5::Widgets Qt5::Qml
           core_fsm
)

add_executable(gui_qt_client_exec
    gui_main.cpp
)

target_link_libraries(gui_qt_client_exec
    PRIVATE gui_qt_client
)

install(TARGETS gui_qt_client_exec RUNTIME DESTINATION bin)
