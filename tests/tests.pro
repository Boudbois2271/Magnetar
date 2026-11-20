# SPDX-License-Identifier: GPL-3.0-or-later
# Unit tests for the core library and the CLI. Run with bin/test; no keyboard needed.
TEMPLATE = app
TARGET = tests
QT = core testlib
CONFIG += console c++20 warn_on
CONFIG -= app_bundle
QMAKE_CXXFLAGS += -Wextra

INCLUDEPATH += ../core ../cli
LIBS += -L$$OUT_PWD/../core -lakkocore
PRE_TARGETDEPS += $$OUT_PWD/../core/libakkocore.a
DEFINES += FIXTURES_DIR=\\\"$$PWD/fixtures\\\"

HEADERS = fake_transport.h
SOURCES = main.cpp tst_core.cpp tst_cli.cpp ../cli/cli.cpp
