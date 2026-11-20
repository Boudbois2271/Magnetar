// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard as QML sees it: its state as properties, changes as invokable actions.
// Every action runs on the DeviceWorker; the properties only ever show what was read back
// from the board.
#pragma once

#include "device_worker.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString errorKind READ errorKind NOTIFY connectionChanged)
    Q_PROPERTY(QString errorDetail READ errorDetail NOTIFY connectionChanged)
    Q_PROPERTY(int profile READ profile NOTIFY stateChanged)
    Q_PROPERTY(int profileCount READ profileCount NOTIFY stateChanged)
    Q_PROPERTY(int effect READ effect NOTIFY stateChanged)
    Q_PROPERTY(bool rainbow READ rainbow NOTIFY stateChanged)
    Q_PROPERTY(QString color READ color NOTIFY stateChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY stateChanged)
    Q_PROPERTY(int maxBrightness READ maxBrightness CONSTANT)
    Q_PROPERTY(int speed READ speed NOTIFY stateChanged)
    Q_PROPERTY(bool lightsOn READ lightsOn NOTIFY stateChanged)
    Q_PROPERTY(QString firmware READ firmware NOTIFY stateChanged)
    Q_PROPERTY(QString devicePath READ devicePath NOTIFY stateChanged)

public:
    explicit Backend(SessionFactory sessions = hidrawSessions(), const QString &stateFile = {},
                     QObject *parent = nullptr);
    ~Backend() override;

    bool connected() const { return m_connected; }
    bool ready() const { return m_ready; }
    // "notFound", "permission", "busy", "noResponse" or "other", so QML can say it in the
    // user's language; errorDetail is the core's own English message.
    QString errorKind() const { return m_errorKind; }
    QString errorDetail() const { return m_errorDetail; }
    int profile() const { return m_state.profile + 1; }
    int profileCount() const { return m_state.profileCount; }
    int effect() const { return m_state.lighting[akko::Lighting::Effect]; }
    bool rainbow() const { return m_state.lighting.rainbow(); }
    QString color() const;
    int brightness() const { return m_state.lighting[akko::Lighting::Brightness]; }
    int maxBrightness() const { return akko::kMaxBrightness; }
    int speed() const { return m_state.lighting[akko::Lighting::Speed]; }
    bool lightsOn() const { return brightness() > 0; }
    QString firmware() const;
    QString devicePath() const { return QString::fromStdString(m_state.path); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setProfile(int profile);  // 1-based, like the UI
    Q_INVOKABLE void setEffect(int effect);
    Q_INVOKABLE bool setColor(const QString &color);  // false (and nothing sent) if it isn't a colour
    Q_INVOKABLE void setRainbow(bool on);
    Q_INVOKABLE void setBrightness(int value);
    Q_INVOKABLE void setSpeed(int value);
    Q_INVOKABLE void setLightsOn(bool on);
    Q_INVOKABLE void toggleLights();

    static QString classifyError(const QString &message);

signals:
    void connectionChanged();
    void stateChanged();

private:
    void post(const std::string &key, DeviceWorker::Action action);
    void applyState(const Snapshot &s);
    void applyError(const QString &message);

    std::string m_stateFile;
    Snapshot m_state;
    bool m_connected = false;
    bool m_ready = false;
    QString m_errorKind;
    QString m_errorDetail;
    QTimer m_retry;
    std::unique_ptr<DeviceWorker> m_worker;  // destroyed first, in ~Backend
};
