// SPDX-License-Identifier: GPL-3.0-or-later
// The Mural audio visualiser, for QML as `mural`: while on, what the desktop plays lights the
// board as spectrum bars (Mural frames, 08 04, streamed by the device worker between actions).
// Nothing is stored on the board: when it stops (off, or the app quits) the board goes back to
// its own effect. The settings are app preferences, kept in the settings file (QSettings), on/off
// included, so the visualiser comes back with the app. The board shows Mural frames as sent, so
// the colours follow its brightness (Lighting): dimmer with it, dark with the lights off.
#pragma once

#include "audio_capture.h"
#include "visualiser.h"

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

#include <memory>

class Backend;

class MuralController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool on READ on WRITE setOn NOTIFY settingsChanged)
    Q_PROPERTY(double sensitivity READ sensitivity WRITE setSensitivity NOTIFY settingsChanged)
    Q_PROPERTY(double sensitivityMin READ sensitivityMin CONSTANT)
    Q_PROPERTY(double sensitivityMax READ sensitivityMax CONSTANT)
    Q_PROPERTY(bool rainbow READ rainbow WRITE setRainbow NOTIFY settingsChanged)
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY settingsChanged)
    // The desktop's audio is open (false while off, or when the sound server can't be reached).
    Q_PROPERTY(bool hearing READ hearing NOTIFY hearingChanged)
    // What is being sent: key id (string) → "#rrggbb", every key while on, empty while off.
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY colorsChanged)

public:
    MuralController(Backend &backend, const QString &settingsFile, AudioCaptureFactory audio = pulseCapture(),
                    QObject *parent = nullptr);
    ~MuralController() override;

    bool on() const { return m_on; }
    void setOn(bool on);
    double sensitivity() const { return m_settings.sensitivity; }
    void setSensitivity(double value);
    double sensitivityMin() const { return Visualiser::kSensitivityMin; }
    double sensitivityMax() const { return Visualiser::kSensitivityMax; }
    bool rainbow() const { return m_settings.rainbow; }
    void setRainbow(bool on);
    QString color() const;
    void setColor(const QString &color);  // ignored if it isn't a colour
    bool hearing() const { return m_hearing; }
    QVariantMap colors() const { return m_colors; }

    // How often the page's preview follows, and how long without samples counts as silence
    // (a suspended output sends none).
    static constexpr int kTickMs = 50;
    static constexpr int kSilentAfterMs = 150;

signals:
    void settingsChanged();
    void hearingChanged();
    void colorsChanged();

private:
    struct Shared;
    void apply();  // starts or stops capture and streaming to match m_on
    void changed();  // stores the settings and hands them to the visualiser
    void tick();
    void followBoard();  // the board's brightness into the colours

    Backend &m_backend;
    AudioCaptureFactory m_audio;
    QSettings m_file;
    bool m_on = false;
    VisualiserSettings m_settings;
    bool m_hearing = false;
    QVariantMap m_colors;
    double m_brightness = 1;
    unsigned m_shown = 0;  // sequence number of the colours in m_colors
    QTimer m_tick;
    std::shared_ptr<Shared> m_shared;     // with the capture thread and the device worker
    std::unique_ptr<AudioCapture> m_capture;  // last: stopped first
};
