#-------------------------------------------------
#
# Project created by QtCreator 2018-01-12T12:54:55
#
#-------------------------------------------------

QT += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CoreTest
TEMPLATE = app

CONFIG += c++17

unix:QMAKE_LFLAGS += -no-pie

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
        main.cpp \
        mainwindow.cpp

HEADERS += \
        mainwindow.h \
    core.h

FORMS += \
        mainwindow.ui
