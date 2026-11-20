// SPDX-License-Identifier: GPL-3.0-or-later
// The akkoctl command line, split from main() so tests can run it against a fake keyboard.
// Output must stay byte-identical to what it was: the Omarchy widget parses it.
#pragma once

#include "keyboard.h"

#include <optional>
#include <string>
#include <vector>

namespace akko::cli {

struct Command {
    enum Kind { Status, Profile, Brightness, Color, On, Off, Toggle } kind = Status;
    int number = 0;     // profile (1-based) or brightness
    std::string value;  // colour
};

// What parsing the arguments decided. When `command` is empty, print `message` and exit with
// `exitCode` (0 for --help, 2 for a usage error, like Python's argparse).
struct Parsed {
    std::optional<Command> command;
    std::string message;
    int exitCode = 0;
};

Parsed parseArgs(const std::vector<std::string> &args);

// ~/.local/state/akkoctl/state.json (or under $XDG_STATE_HOME), and the log beside it.
std::string defaultStateFile();
std::string logFileFor(const std::string &stateFile);

// Run one command on the keyboard and return the status line (JSON, newline included).
// Throws akko::Error, whose message becomes the error line.
std::string execute(Keyboard &kb, const Command &cmd, const std::string &stateFile);

// {"connected": false, "error": "..."} plus newline.
std::string errorJson(const std::string &message);

// Python's json.dumps for a string (ensure_ascii): quotes, escapes, \uXXXX above ASCII.
std::string jsonString(const std::string &utf8);

// Append one line to the call log, rotating it to .1 past 256 KiB. Never fails.
void logCall(const std::string &logFile, const std::vector<std::string> &args, const std::string &result,
             double elapsedMs);

}  // namespace akko::cli
