// SPDX-License-Identifier: GPL-3.0-or-later
// akkoctl: profile and lighting control for the Akko TAC75 HE, printing the state as JSON.
#include "cli.h"
#include "device.h"

#include <chrono>
#include <cstdio>
#include <exception>

int main(int argc, char **argv)
{
    using namespace akko;
    const std::vector<std::string> args(argv + 1, argv + argc);

    const cli::Parsed parsed = cli::parseArgs(args);
    if (!parsed.command) {
        std::fputs(parsed.message.c_str(), parsed.exitCode == 0 ? stdout : stderr);
        return parsed.exitCode;
    }

    const auto start = std::chrono::steady_clock::now();
    const std::string stateFile = cli::defaultStateFile();
    std::string result = "ok";
    int code = 0;
    try {
        Connection connection;  // takes the session lock, then opens the keyboard
        Keyboard kb(connection.transport());
        std::fputs(cli::execute(kb, *parsed.command, stateFile).c_str(), stdout);
    } catch (const std::exception &e) {
        // Any failure still prints one JSON line, so the widget always has something to parse.
        result = std::string("error: ") + e.what();
        std::fputs(cli::errorJson(e.what()).c_str(), stdout);
        code = 1;
    }
    const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
    cli::logCall(cli::logFileFor(stateFile), args, result, elapsed.count());
    return code;
}
