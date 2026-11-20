// SPDX-License-Identifier: GPL-3.0-or-later
// One test binary for every suite; the exit code is non-zero if any of them failed.
#include <QGuiApplication>

int runCoreTests(int argc, char **argv);
int runCliTests(int argc, char **argv);
int runAppTests(int argc, char **argv);

int main(int argc, char **argv)
{
    // The app suite loads the real window: draw it offscreen, never on the desktop.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    int failed = 0;
    failed += runCoreTests(argc, argv) != 0;
    failed += runCliTests(argc, argv) != 0;
    failed += runAppTests(argc, argv) != 0;
    return failed;
}
