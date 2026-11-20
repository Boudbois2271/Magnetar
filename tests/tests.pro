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
    ../app/src/backend.h ../app/src/device_worker.h ../app/src/theme.h ../app/src/translator.h
SOURCES = main.cpp tst_core.cpp tst_cli.cpp tst_app.cpp ../cli/cli.cpp \
    ../app/src/backend.cpp ../app/src/device_worker.cpp ../app/src/theme.cpp ../app/src/translator.cpp
RESOURCES = ../app/src/resources.qrc
