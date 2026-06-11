QT       += core gui widgets network concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mac_lookup
TEMPLATE = app
CONFIG += utf8_source

QMAKE_CXXFLAGS += -Wall -Wextra
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    scanner.cpp \
    oui_database.cpp \
    thememanager.cpp \
    fingerprint.cpp \
    wol.cpp \
    netdiag.cpp \
    importer.cpp \
    speedtest.cpp \
    bandwidth.cpp \
    vulnscan.cpp

HEADERS += \
    mainwindow.h \
    scanner.h \
    oui_database.h \
    thememanager.h \
    fingerprint.h \
    wol.h \
    netdiag.h \
    importer.h \
    speedtest.h \
    bandwidth.h \
    vulnscan.h

win32:RC_FILE = app_icon.rc
win32:LIBS += -liphlpapi -lws2_32 -lwlanapi -lole32

RESOURCES += icons.qrc

DISTFILES += \
    data/mac_database.txt
