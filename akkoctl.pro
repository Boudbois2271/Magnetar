# SPDX-License-Identifier: GPL-3.0-or-later
# Build everything with bin/build (qmake6 + make, out of tree in build/).
TEMPLATE = subdirs
SUBDIRS = core cli tests
cli.depends = core
tests.depends = core
