# SPDX-License-Identifier: GPL-3.0-or-later
# The protocol library shared by the CLI and the app. Plain C++ and POSIX, no Qt.
TEMPLATE = lib
TARGET = akkocore
CONFIG += staticlib c++20 warn_on
CONFIG -= qt
QMAKE_CXXFLAGS += -Wextra

HEADERS = frame.h transport.h device.h keyboard.h keymap.h lights.h travel.h advanced.h macro.h calibration.h mural.h
SOURCES = frame.cpp device.cpp keyboard.cpp keymap.cpp lights.cpp travel.cpp advanced.cpp macro.cpp calibration.cpp mural.cpp
