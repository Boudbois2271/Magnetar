# SPDX-License-Identifier: GPL-3.0-or-later
# The akkoctl command line. Built without Qt, so the binary has no runtime dependencies.
TEMPLATE = app
TARGET = akkoctl
CONFIG += console c++20 warn_on
CONFIG -= qt app_bundle
QMAKE_CXXFLAGS += -Wextra

INCLUDEPATH += ../core
LIBS += -L$$OUT_PWD/../core -lakkocore
PRE_TARGETDEPS += $$OUT_PWD/../core/libakkocore.a

HEADERS = cli.h
SOURCES = cli.cpp main.cpp
