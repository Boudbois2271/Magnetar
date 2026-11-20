// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend.h"

#include "lights.h"

#include <QMetaObject>

Backend::Backend(SessionFactory sessions, const QString &stateFile, QObject *parent)
    : QObject(parent),
      m_stateFile(stateFile.isEmpty() ? akko::defaultStateFile() : stateFile.toStdString())
{
    // While the keyboard is unreachable, try again every few seconds, so plugging it in or
    // closing the vendor web app is enough.
    m_retry.setInterval(3000);
    connect(&m_retry, &QTimer::timeout, this, &Backend::refresh);

    // The worker calls back on its own thread; hop to this one before touching state.
    m_worker = std::make_unique<DeviceWorker>(
        std::move(sessions),
        [this](const Snapshot &s) { QMetaObject::invokeMethod(this, [this, s] { applyState(s); }, Qt::QueuedConnection); },
        [this](const std::string &e) {
            const QString message = QString::fromStdString(e);
            QMetaObject::invokeMethod(this, [this, message] { applyError(message); }, Qt::QueuedConnection);
        });
    refresh();
}

Backend::~Backend()
{
    m_worker.reset();  // joins the thread before the members its callbacks use go away
}

QString Backend::color() const
{
    const auto &l = m_state.lighting;
    return QString::asprintf("#%02x%02x%02x", l[akko::Lighting::Red], l[akko::Lighting::Green],
                             l[akko::Lighting::Blue]);
}

QString Backend::firmware() const
{
    return m_state.firmware ? QString::fromStdString(akko::formatFirmware(m_state.firmware)) : QString();
}

void Backend::post(const std::string &key, DeviceWorker::Action action)
{
    m_worker->post(key, std::move(action));
}

void Backend::refresh()
{
    post("refresh", [](akko::Keyboard &) {});  // every batch ends with a full read
}

void Backend::setProfile(int profile)
{
    if (profile < 1 || (m_state.profileCount && profile > m_state.profileCount))
        return;
    post("profile", [profile](akko::Keyboard &kb) { kb.setProfile(profile - 1); });
}

void Backend::setEffect(int effect)
{
    if (effect < 0 || effect > 0x13)
        return;
    post("effect", [effect](akko::Keyboard &kb) { kb.setEffect(effect); });
}

bool Backend::setColor(const QString &color)
{
    std::array<std::uint8_t, 3> rgb;
    try {
        rgb = akko::parseColor(color.toStdString());
    } catch (const akko::Error &) {
        return false;
    }
    post("color", [rgb](akko::Keyboard &kb) { akko::setColor(kb, rgb); });
    return true;
}

void Backend::setRainbow(bool on)
{
    post("rainbow", [on](akko::Keyboard &kb) { akko::setRainbow(kb, on); });
}

void Backend::setBrightness(int value)
{
    const std::string file = m_stateFile;
    post("brightness", [value, file](akko::Keyboard &kb) { akko::setBrightness(kb, value, file); });
}

void Backend::setSpeed(int value)
{
    post("speed", [value](akko::Keyboard &kb) { akko::setSpeed(kb, value); });
}

void Backend::setLightsOn(bool on)
{
    const std::string file = m_stateFile;
    const auto action = on ? akko::LightsAction::On : akko::LightsAction::Off;
    post("lights", [action, file](akko::Keyboard &kb) { akko::switchLights(kb, action, file); });
}

void Backend::toggleLights()
{
    // Not keyed: two quick toggles must both happen, not merge into one.
    const std::string file = m_stateFile;
    post({}, [file](akko::Keyboard &kb) { akko::switchLights(kb, akko::LightsAction::Toggle, file); });
}

QString Backend::classifyError(const QString &message)
{
    if (message.startsWith("keyboard not found"))
        return "notFound";
    if (message.startsWith("no permission"))
        return "permission";
    if (message.startsWith("keyboard busy"))
        return "busy";
    if (message.startsWith("no response") || message.startsWith("keyboard disconnected"))
        return "noResponse";
    return "other";
}

void Backend::applyState(const Snapshot &s)
{
    m_state = s;
    m_ready = true;
    m_retry.stop();
    if (!m_connected || !m_errorKind.isEmpty()) {
        m_connected = true;
        m_errorKind.clear();
        m_errorDetail.clear();
        emit connectionChanged();
    }
    emit stateChanged();
}

void Backend::applyError(const QString &message)
{
    m_connected = false;
    m_errorKind = classifyError(message);
    m_errorDetail = message;
    m_retry.start();
    emit connectionChanged();
}
