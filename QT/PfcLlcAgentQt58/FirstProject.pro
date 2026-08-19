QT += core gui widgets serialport charts network

CONFIG += c++14
TEMPLATE = app
TARGET = PfcLlcAgentQt58

# Build with Qt 5.15 / MSVC2019. Keep source text explicitly UTF-8.
win32-msvc*: QMAKE_CXXFLAGS -= -Za
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

SOURCES += \
    main.cpp \
    telemetrysnapshot.cpp \
    modbusprotocol.cpp \
    modbusstreamparser.cpp \
    serialmodbusclient.cpp \
    localruleengine.cpp \
    agentdecision.cpp \
    mimoclient.cpp \
    configmanager.cpp \
    safetygate.cpp \
    widget.cpp

HEADERS += \
    telemetrysnapshot.h \
    modbusprotocol.h \
    modbusstreamparser.h \
    serialmodbusclient.h \
    localruleengine.h \
    agentdecision.h \
    mimoclient.h \
    configmanager.h \
    safetygate.h \
    widget.h

# This is a Qt 5.8 PFC/LLC-only project. The former battery/BMS Designer and
# communication sources are intentionally removed from this working copy.
