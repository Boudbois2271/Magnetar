# SPDX-License-Identifier: GPL-3.0-or-later
# Build everything with bin/build (qmake6 + make, out of tree in build/).
TEMPLATE = subdirs
SUBDIRS = core cli app tests
cli.depends = core
app.depends = core
tests.depends = core
