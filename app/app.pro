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

HEADERS = src/backend.h src/device_worker.h src/desktop_size.h src/theme.h src/translator.h src/scancodes.h src/color_palette.h src/visualiser.h src/audio_capture.h src/mural_controller.h src/hotplug_watch.h
SOURCES = src/main.cpp src/backend.cpp src/device_worker.cpp src/desktop_size.cpp src/theme.cpp src/translator.cpp src/scancodes.cpp src/color_palette.cpp src/visualiser.cpp src/audio_capture.cpp src/mural_controller.cpp src/hotplug_watch.cpp
LIBS += -lpulse-simple -lpulse -ludev
RESOURCES = src/resources.qrc
