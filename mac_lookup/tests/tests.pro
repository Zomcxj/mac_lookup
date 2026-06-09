QT       += testlib core gui widgets network concurrent
QT       -= app_bundle

TARGET = mac_lookup_test
CONFIG   += console c++11
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += \
    test_main.cpp \
    test_utils.cpp

HEADERS += \
    test_utils.h

DEFINES += QT_DEPRECATED_WARNINGS
