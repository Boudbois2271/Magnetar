// SPDX-License-Identifier: GPL-3.0-or-later
// The desktop's text size, for QML as `ui`: every font, gap and control size in the app derives
// from it, so the app follows the desktop's "text size" setting like GTK apps do.
//
// On Omarchy (and GNOME) one setting drives text size: org.gnome.desktop.interface
// text-scaling-factor, which `omarchy display text size` sets. GTK applies it to the interface
// font; Qt's gtk3 platform theme hands Qt that font but not the factor. So the base size here is
// the platform's interface font times that factor, read with `gsettings` and kept up to date.
#pragma once

#include <QFileSystemWatcher>
#include <QFont>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <cmath>

class DesktopSize : public QObject {
    Q_OBJECT
    // The interface font at the desktop's size: family and pixel size only, so Material keeps
    // its own weights (medium buttons, bold headings).
    Q_PROPERTY(QFont font READ font NOTIFY changed)
    // Body text, in pixels.
    Q_PROPERTY(int fontSize READ fontSize NOTIFY changed)
    // One layout pixel: the app's layout was drawn for 14 px body text (Material's size), so
    // `n * ui.unit` is n at that size and shrinks or grows with the desktop's text.
    Q_PROPERTY(qreal unit READ unit NOTIFY changed)
    // The type scale, in pixels (n at 14 px body): page title 22 and section title 15 (both
    // semibold in QML), body 14, small 12 for secondary lines.
    Q_PROPERTY(int title READ titleSize NOTIFY changed)
    Q_PROPERTY(int section READ sectionSize NOTIFY changed)
    Q_PROPERTY(int body READ fontSize NOTIFY changed)
    Q_PROPERTY(int small READ smallSize NOTIFY changed)

public:
    // `gsettings`: the program to ask for the text scaling factor; empty finds it on PATH, a
    // missing one means a factor of 1.
    explicit DesktopSize(const QString &gsettings = {}, QObject *parent = nullptr);
    ~DesktopSize() override;

    QFont font() const;
    int fontSize() const { return m_fontSize; }
    qreal unit() const { return m_fontSize / 14.0; }
    int titleSize() const { return scaled(22); }
    int sectionSize() const { return scaled(15); }
    int smallSize() const { return scaled(12); }

    // Uses this interface font and factor instead of the platform's (tests, screenshots).
    void setBase(const QFont &font, double textScaling);

    // Pixel size of `font` scaled by `textScaling`, at `dpi` (points are 1/72 inch).
    static int pixelSize(const QFont &font, double textScaling, double dpi);
    // `gsettings get` output, e.g. "0.81820000000000004"; 1 for anything else.
    static double parseScaling(const QByteArray &output);

signals:
    void changed();

private:
    int scaled(int n) const { return int(std::lround(n * unit())); }
    void query();
    void update();

    QFont m_base;
    double m_scaling = 1;
    int m_fontSize = 14;
    QString m_gsettings;
    QProcess m_process;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
