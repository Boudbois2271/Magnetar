// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_size.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <cmath>

namespace {
const QStringList scalingKey{"get", "org.gnome.desktop.interface", "text-scaling-factor"};

double screenDpi()
{
    const auto *screen = QGuiApplication::primaryScreen();
    return screen ? screen->logicalDotsPerInch() : 96;
}
}  // namespace

DesktopSize::DesktopSize(const QString &gsettings, QObject *parent)
    : QObject(parent),
      m_base(QGuiApplication::font()),
      m_gsettings(gsettings.isEmpty() ? QStandardPaths::findExecutable("gsettings") : gsettings)
{
    // The first answer is waited for, so the window opens at the right size.
    if (QFileInfo(m_gsettings).isExecutable()) {
        m_process.start(m_gsettings, scalingKey);
        if (m_process.waitForFinished(1000) && m_process.exitStatus() == QProcess::NormalExit
            && m_process.exitCode() == 0)
            m_scaling = parseScaling(m_process.readAllStandardOutput());
        else
            m_process.kill();
        m_process.waitForFinished(100);
    }
    update();

    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (code == 0 && status == QProcess::NormalExit) {
            m_scaling = parseScaling(m_process.readAllStandardOutput());
            update();
        }
    });
    // A text size change rewrites dconf's database; ask again then. dconf replaces the file,
    // so the folder is watched too.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &DesktopSize::query);
    const auto dconf = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/dconf";
    if (QFileInfo(m_gsettings).isExecutable() && QFileInfo::exists(dconf)) {
        m_watcher.addPath(dconf);
        if (QFileInfo::exists(dconf + "/user"))
            m_watcher.addPath(dconf + "/user");
        auto changed = [this, dconf] {
            if (!m_watcher.files().contains(dconf + "/user") && QFileInfo::exists(dconf + "/user"))
                m_watcher.addPath(dconf + "/user");
            m_debounce.start();
        };
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, changed);
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, changed);
    }
}

DesktopSize::~DesktopSize()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QFont DesktopSize::font() const
{
    QFont f;
    f.setFamilies(m_base.families().isEmpty() ? QStringList{m_base.family()} : m_base.families());
    f.setPixelSize(m_fontSize);
    return f;
}

void DesktopSize::setBase(const QFont &font, double textScaling)
{
    m_gsettings.clear();
    if (const auto paths = m_watcher.files() + m_watcher.directories(); !paths.isEmpty())
        m_watcher.removePaths(paths);
    m_base = font;
    m_scaling = textScaling;
    update();
}

void DesktopSize::query()
{
    if (m_gsettings.isEmpty() || m_process.state() != QProcess::NotRunning)
        return;
    m_process.start(m_gsettings, scalingKey);
}

void DesktopSize::update()
{
    const int size = pixelSize(m_base, m_scaling, screenDpi());
    if (size != m_fontSize) {
        m_fontSize = size;
        emit changed();
    }
}

int DesktopSize::pixelSize(const QFont &font, double textScaling, double dpi)
{
    const double px = font.pixelSize() > 0 ? font.pixelSize() : font.pointSizeF() * dpi / 72;
    return std::max(6, int(std::lround(px * textScaling)));
}

double DesktopSize::parseScaling(const QByteArray &output)
{
    bool ok = false;
    const double v = output.trimmed().toDouble(&ok);
    return ok && v >= 0.5 && v <= 3 ? v : 1;
}
