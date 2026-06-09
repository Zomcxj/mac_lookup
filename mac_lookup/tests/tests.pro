QT       += testlib core gui widgets network concurrent
QT       -= app_bundle

TARGET = mac_lookup_test
CONFIG   += console c++11
CONFIG   -= app_bundle

TEMPLATE = app

QMAKE_CXXFLAGS += -Wall -Wextra
SOURCES += \
    test_main.cpp \
    test_utils.cpp \
    ../scanner.cpp \
    ../oui_database.cpp \
    ../thememanager.cpp

HEADERS += \
    test_utils.h \
    ../scanner.h \
    ../oui_database.h \
    ../thememanager.h

DEFINES += QT_DEPRECATED_WARNINGS
win32:LIBS += -liphlpapi -lws2_32 -lwlanapi -lole32
