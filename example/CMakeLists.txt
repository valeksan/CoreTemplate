cmake_minimum_required(VERSION 3.16)

project(ExampleApp LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Creating an executable file
add_executable(ExampleApp
    main.cpp
    mainwindow.cpp
    mainwindow.ui
    mainwindow.h
    ../core.h
)

# Adding the directory with core.h in the header search path
target_include_directories(ExampleApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Connecting the Qt libraries
target_link_libraries(ExampleApp PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# Enabling automatic generation
set_target_properties(ExampleApp PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

# Setting the C++ standard
target_compile_features(ExampleApp PRIVATE cxx_std_17)

# Checking core.h
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../core.h")
    message(FATAL_ERROR "core.h not found at expected path: ${CMAKE_CURRENT_SOURCE_DIR}/../core.h")
endif()
