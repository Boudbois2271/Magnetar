# SPDX-License-Identifier: GPL-3.0-or-later
# Tests for the core library, the CLI and the app. Run with bin/test; no keyboard needed.
TEMPLATE = app
TARGET = tests
QT = core gui qml quick quickcontrols2 testlib
CONFIG += console c++20 warn_on
CONFIG -= app_bundle
QMAKE_CXXFLAGS += -Wextra

INCLUDEPATH += ../core ../cli ../app/src
LIBS += -L$$OUT_PWD/../core -lakkocore
PRE_TARGETDEPS += $$OUT_PWD/../core/libakkocore.a
DEFINES += FIXTURES_DIR=\\\"$$PWD/fixtures\\\" APP_SRC_DIR=\\\"$$PWD/../app/src\\\"

HEADERS = fake_transport.h \
    ../app/src/backend.h ../app/src/device_worker.h ../app/src/desktop_size.h ../app/src/theme.h ../app/src/translator.h ../app/src/scancodes.h ../app/src/color_palette.h ../app/src/visualiser.h ../app/src/audio_capture.h ../app/src/mural_controller.h ../app/src/hotplug_watch.h ../app/src/report_listener.h ../app/src/keyboard_layout.h
SOURCES = main.cpp tst_core.cpp tst_cli.cpp tst_app.cpp ../cli/cli.cpp \
    ../app/src/backend.cpp ../app/src/device_worker.cpp ../app/src/desktop_size.cpp ../app/src/theme.cpp ../app/src/translator.cpp ../app/src/scancodes.cpp ../app/src/color_palette.cpp ../app/src/visualiser.cpp ../app/src/audio_capture.cpp ../app/src/mural_controller.cpp ../app/src/hotplug_watch.cpp ../app/src/report_listener.cpp ../app/src/keyboard_layout.cpp
LIBS += -lpulse-simple -lpulse -ludev -lxkbcommon -lwayland-client
RESOURCES = ../app/src/resources.qrc
