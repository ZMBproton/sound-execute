cmake_minimum_required(VERSION 3.19)
project(sound_execute VERSION 0.1 LANGUAGES CXX)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia Sql Svg DBus)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
qt_standard_project_setup()

qt_add_executable(sound_execute
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    database.h
    database.cpp
    resources.qrc
    mpris.h
)

target_link_libraries(sound_execute
    PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::Sql
        Qt6::Svg
        Qt6::DBus
)

install(TARGETS sound_execute
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET sound_execute
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
set_target_properties(sound_execute PROPERTIES
    OUTPUT_NAME "sound-execute"
)
install(SCRIPT ${deploy_script})
