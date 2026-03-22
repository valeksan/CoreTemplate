cmake_minimum_required(VERSION 3.16)

project(CoreTemplateTests LANGUAGES CXX)

enable_testing()

set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Test)

add_executable(CoreTemplateTests
    core_tests.cpp
    ../core.h
)

target_include_directories(CoreTemplateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(CoreTemplateTests PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

target_compile_features(CoreTemplateTests PRIVATE cxx_std_17)

add_test(NAME CoreTemplateTests COMMAND CoreTemplateTests)

