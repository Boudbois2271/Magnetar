# SPDX-License-Identifier: GPL-3.0-or-later
# Magnetar, the desktop app: Qt Quick on top of the core library.
TEMPLATE = app
TARGET = magnetar
QT += core gui qml quick quickcontrols2
CONFIG += c++20 warn_on release
QMAKE_CXXFLAGS += -Wextra

INCLUDEPATH += ../core
LIBS += -L$$OUT_PWD/../core -lakkocore
PRE_TARGETDEPS += $$OUT_PWD/../core/libakkocore.a

HEADERS = src/backend.h src/device_worker.h src/theme.h src/translator.h
SOURCES = src/main.cpp src/backend.cpp src/device_worker.cpp src/theme.cpp src/translator.cpp
RESOURCES = src/resources.qrc
