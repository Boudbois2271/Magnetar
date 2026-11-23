// SPDX-License-Identifier: MIT
// From the Omarchy app templates, which follow Omacut and Monologue.
// Copyright (c) David Heinemeier Hansson. Permission is hereby granted, free of charge, to any
// person obtaining a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
// to permit persons to whom the Software is furnished to do so, subject to the following
// conditions: The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
// WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
// OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    // Whole-palette roles from colors.toml. Surfaces are shades between background
    // and foreground, so they read on light and dark themes alike.
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
    // Secondary text: foreground mixed 65 % into background, readable where opacity wasn't.
    Q_PROPERTY(QColor secondary READ secondary NOTIFY changed)
    Q_PROPERTY(QColor sidebar READ sidebar NOTIFY changed)
    Q_PROPERTY(QColor card READ card NOTIFY changed)
    Q_PROPERTY(QColor overlay READ overlay NOTIFY changed)
    Q_PROPERTY(QColor chip READ chip NOTIFY changed)
    Q_PROPERTY(QColor chipHover READ chipHover NOTIFY changed)
    Q_PROPERTY(QColor key READ key NOTIFY changed)
    Q_PROPERTY(QColor keyHover READ keyHover NOTIFY changed)
    Q_PROPERTY(QColor keyBorder READ keyBorder NOTIFY changed)
    Q_PROPERTY(QColor banner READ banner NOTIFY changed)
    Q_PROPERTY(QColor bannerError READ bannerError NOTIFY changed)
    Q_PROPERTY(QColor error READ error NOTIFY changed)
    Q_PROPERTY(QColor success READ success NOTIFY changed)
    Q_PROPERTY(QColor warning READ warning NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }

    bool dark() const;
    QColor background() const { return color("background", "#0e0e10"); }
    QColor foreground() const { return color("foreground", "#ffffff"); }
    QColor secondary() const;
    QColor sidebar() const { return shade(.025, "#141417"); }
    QColor card() const { return shade(.04, "#17171b"); }
    QColor overlay() const { return shade(.055, "#1b1b20"); }
    QColor chip() const { return shade(.08, "#222228"); }
    QColor chipHover() const { return shade(.12, "#2c2c34"); }
    QColor key() const { return shade(.07, "#202026"); }
    QColor keyHover() const { return shade(.11, "#2a2a31"); }
    QColor keyBorder() const { return shade(.14, "#2e2e36"); }
    QColor banner() const { return shade(.06, "#1d1d22"); }
    QColor bannerError() const;
    QColor error() const { return color("red", "#ff8a80"); }
    QColor success() const { return color("green", "#3f8f50"); }
    QColor warning() const { return color("yellow", "#8a6a1c"); }

    static QString readAccent(const QString &path);
    // Every `key = "value"` line of a colors.toml; empty when the file is missing.
    static QHash<QString, QString> readValues(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();
    QColor color(const QString &key, const char *fallback) const;
    QColor shade(double towardForeground, const char *fallback) const;

    QString m_directory;
    QString m_accent = "#FFD60A";
    QHash<QString, QString> m_values;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
