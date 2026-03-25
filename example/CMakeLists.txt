cmake_minimum_required(VERSION 3.16)

project(ExampleApp LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

# Creating an executable file
add_executable(ExampleApp
    main.cpp
    mainwindow.cpp
    mainwindow.ui
    mainwindow.h
    ../core.h
)

add_executable(ExampleConsoleApp
    console_main.cpp
    ../core.h
)

# Adding the directory with core.h in the header search path
target_include_directories(ExampleApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(ExampleConsoleApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Connecting the Qt libraries
target_link_libraries(ExampleApp PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

target_link_libraries(ExampleConsoleApp PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
)

# Setting the C++ standard
target_compile_features(ExampleApp PRIVATE cxx_std_17)
target_compile_features(ExampleConsoleApp PRIVATE cxx_std_17)

# Checking core.h
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../core.h")
    message(FATAL_ERROR "core.h not found at expected path: ${CMAKE_CURRENT_SOURCE_DIR}/../core.h")
endif()
