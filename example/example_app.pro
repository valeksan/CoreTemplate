# example/example_app.pro

# Defining a variable for the library path for example
CORE_PATH = $$PWD/../

# Add Qt modules
QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Config compile
CONFIG += c++17
unix:QMAKE_LFLAGS += -no-pie
DEFINES += QT_DEPRECATED_WARNINGS
INCLUDEPATH += $$CORE_PATH

# Config project
TARGET = ExampleApp
TEMPLATE = app

# Files
SOURCES += \
        main.cpp \
        mainwindow.cpp

HEADERS += \
        mainwindow.h \
        $$CORE_PATH/core.h

FORMS += \
        mainwindow.ui
