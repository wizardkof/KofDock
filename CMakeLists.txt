cmake_minimum_required(VERSION 3.21)
project(KofDock VERSION 0.2.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 DBus)
find_package(LayerShellQt 6 REQUIRED CONFIG)

qt_standard_project_setup(REQUIRES 6.8)
# Qt is shared on the supported Garuda/Arch runtime. Load QML imports
# dynamically from the installed system instead of attempting to link the
# distro's Kirigami/taskmanager plugins as in-tree CMake targets.
# Standard add_executable avoids Qt's unnecessary static plugin import scan.
add_executable(kofdock
    src/main.cpp
    src/docksettings.h
    src/docksettings.cpp
    src/dockautostart.h
    src/dockautostart.cpp
    src/dockcontroller.h
    src/dockcontroller.cpp
    src/screenrouting.h
    src/docksectionmodel.h
    src/docksectionmodel.cpp
    src/dockservices.h
    src/dockservices.cpp
)

qt_add_resources(kofdock "dock_qml"
    PREFIX "/kofdock"
    FILES
        qml/Main.qml
        qml/DockCell.qml
        qml/SettingsWindow.qml
        qml/StackCell.qml
        qml/FanStack.qml
        qml/DockLogic.js
)

target_link_libraries(kofdock PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::DBus
    LayerShellQt::Interface
)

install(TARGETS kofdock RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 COMPONENTS Test QUIET)
    if(TARGET Qt6::Test)
        add_executable(kofdock_section_tests
            tests/test_docksectionmodel.cpp
            src/docksectionmodel.h
            src/docksectionmodel.cpp)
        target_include_directories(kofdock_section_tests PRIVATE src)
        target_link_libraries(kofdock_section_tests PRIVATE Qt6::Core Qt6::Test)
        add_test(NAME dock_sections COMMAND kofdock_section_tests)
        add_executable(kofdock_screen_tests tests/test_screenrouting.cpp src/screenrouting.h)
        target_include_directories(kofdock_screen_tests PRIVATE src)
        target_link_libraries(kofdock_screen_tests PRIVATE Qt6::Core Qt6::Test)
        add_test(NAME dock_screen_routing COMMAND kofdock_screen_tests)
        add_executable(kofdock_autostart_tests tests/test_dockautostart.cpp
                       src/dockautostart.h src/dockautostart.cpp)
        target_include_directories(kofdock_autostart_tests PRIVATE src)
        target_link_libraries(kofdock_autostart_tests PRIVATE Qt6::Core Qt6::Test)
        add_test(NAME dock_autostart COMMAND kofdock_autostart_tests)
    endif()
    find_program(NODE_EXECUTABLE node)
    if(NODE_EXECUTABLE)
        add_test(NAME dock_parabolic_reorder
                 COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dock_logic.cjs")
        add_test(NAME dock_fan_visual_geometry
                 COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fan_layout.cjs")
        add_test(NAME dock_hover_order_stability
                 COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_hover_order.cjs")
        add_test(NAME dock_launch_safety
                 COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_launch_safety.cjs")
    endif()
endif()
