cmake_minimum_required(VERSION 3.16)
project(JumpAJump LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

qt_standard_project_setup()

if(WIN32)
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/JumpAJump.rc")
endif()

qt_add_executable(JumpAJump
    main.cpp
    GameWidget.h GameWidget.cpp
    AudioEngine.h AudioEngine.cpp
    ${APP_ICON_RESOURCE}
)

target_link_libraries(JumpAJump PRIVATE Qt6::Widgets Qt6::Multimedia)

set_target_properties(JumpAJump PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "JumpAJump"
)
