QT       += core gui widgets network concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mac_lookup
TEMPLATE = app
CONFIG += utf8_source

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

win32:RC_FILE = app_icon.rc
win32:LIBS += -liphlpapi -lws2_32 -lwlanapi -lole32

RESOURCES += icons.qrc

DISTFILES += \
    data/mac_database.txt
