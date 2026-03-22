TEMPLATE = app
TARGET = CoreTemplateTests

CONFIG += console testcase c++17
QT += core testlib

SOURCES += core_tests.cpp
HEADERS += ../core.h
INCLUDEPATH += ..
