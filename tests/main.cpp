// SPDX-License-Identifier: GPL-3.0-or-later
// One test binary for every suite; the exit code is non-zero if any of them failed.
#include <QCoreApplication>

int runCoreTests(int argc, char **argv);
int runCliTests(int argc, char **argv);

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failed = 0;
    failed += runCoreTests(argc, argv) != 0;
    failed += runCliTests(argc, argv) != 0;
    return failed;
}
