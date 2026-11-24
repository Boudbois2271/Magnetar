// SPDX-License-Identifier: GPL-3.0-or-later
#include "mural_controller.h"

#include "backend.h"
#include "color_palette.h"

#include <QColor>

#include <chrono>
#include <mutex>

// Shared between the capture thread (feeds samples), the device worker (takes colours) and the
// UI thread (settings, silence, preview).
struct MuralController::Shared {
    using clock = std::chrono::steady_clock;
    std::mutex mutex;
    Visualiser visualiser;
    std::vector<akko::KeyColor> colors = visualiser.colors();
    unsigned sequence = 1;  // bumped whenever colors change
    clock::time_point lastSamples = clock::now();

    void feed(std::span<const float> samples)  // mutex held
    {
        if (visualiser.feed(samples)) {
            colors = visualiser.colors();
            ++sequence;
        }
    }
};

MuralController::MuralController(Backend &backend, const QString &settingsFile, AudioCaptureFactory audio,
                                 QObject *parent)
    : QObject(parent), m_backend(backend), m_audio(std::move(audio)), m_file(settingsFile, QSettings::IniFormat),
      m_shared(std::make_shared<Shared>())
{
    m_file.beginGroup("mural");
    m_settings.sensitivity = std::clamp(m_file.value("sensitivity", 1.0).toDouble(), Visualiser::kSensitivityMin,
                                        Visualiser::kSensitivityMax);
    m_settings.rainbow = m_file.value("rainbow", true).toBool();
    const QColor c(m_file.value("color", "#ffffff").toString());
    if (c.isValid())
        m_settings.color = {std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue())};
    m_shared->visualiser.setSettings(m_settings);
    m_on = m_file.value("on", false).toBool();

    m_tick.setInterval(kTickMs);
    connect(&m_tick, &QTimer::timeout, this, &MuralController::tick);
    apply();
}

MuralController::~MuralController()
{
    m_backend.setMural({});
    m_capture.reset();
}

QString MuralController::color() const
{
    return QString::asprintf("#%02x%02x%02x", m_settings.color[0], m_settings.color[1], m_settings.color[2]);
}

void MuralController::setOn(bool on)
{
    if (on == m_on)
        return;
    m_on = on;
    m_file.setValue("on", on);
    apply();
    emit settingsChanged();
}

void MuralController::setSensitivity(double value)
{
    value = std::clamp(value, Visualiser::kSensitivityMin, Visualiser::kSensitivityMax);
    if (value == m_settings.sensitivity)
        return;
    m_settings.sensitivity = value;
    changed();
}

void MuralController::setRainbow(bool on)
{
    if (on == m_settings.rainbow)
        return;
    m_settings.rainbow = on;
    changed();
}

void MuralController::setColor(const QString &color)
{
    const QString hex = ColorPalette::normalized(color);
    if (hex.isEmpty() || hex == this->color())
        return;
    const QColor c(hex);
    m_settings.color = {std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue())};
    changed();
}

void MuralController::changed()
{
    m_file.setValue("sensitivity", m_settings.sensitivity);
    m_file.setValue("rainbow", m_settings.rainbow);
    m_file.setValue("color", color());
    {
        std::lock_guard lock(m_shared->mutex);
        m_shared->visualiser.setSettings(m_settings);
        m_shared->colors = m_shared->visualiser.colors();
        ++m_shared->sequence;
    }
    emit settingsChanged();
    if (m_on)
        tick();
}

void MuralController::apply()
{
    if (!m_on) {
        m_backend.setMural({});
        m_capture.reset();
        m_tick.stop();
        {
            std::lock_guard lock(m_shared->mutex);
            m_shared->visualiser.reset();
            m_shared->colors = m_shared->visualiser.colors();
            ++m_shared->sequence;
        }
        if (std::exchange(m_hearing, false))
            emit hearingChanged();
        if (!m_colors.isEmpty()) {
            m_colors.clear();
            emit colorsChanged();
        }
        return;
    }
    const auto shared = m_shared;
    m_capture = m_audio(
        [shared](std::span<const float> samples) {
            std::lock_guard lock(shared->mutex);
            shared->lastSamples = Shared::clock::now();
            shared->feed(samples);
        },
        [this](bool hearing) {
            QMetaObject::invokeMethod(this, [this, hearing] {
                if (m_on && hearing != m_hearing) {
                    m_hearing = hearing;
                    emit hearingChanged();
                }
            }, Qt::QueuedConnection);
        });
    m_backend.setMural([shared, last = 0u](std::vector<akko::KeyColor> &colors) mutable {
        std::lock_guard lock(shared->mutex);
        colors = shared->colors;
        return std::exchange(last, shared->sequence) != shared->sequence;
    });
    m_tick.start();
    tick();
}

void MuralController::tick()
{
    std::vector<akko::KeyColor> colors;
    {
        std::lock_guard lock(m_shared->mutex);
        // No samples lately (a suspended output sends none): that is silence, let the bars fall.
        const auto now = Shared::clock::now();
        if (now - m_shared->lastSamples > std::chrono::milliseconds(kSilentAfterMs))
            m_shared->feed(std::vector<float>(Visualiser::kRate * kTickMs / 1000));
        if (m_shared->sequence == m_shown && !m_colors.isEmpty())
            return;
        m_shown = m_shared->sequence;
        colors = m_shared->colors;
    }
    QVariantMap map;
    for (const auto &c : colors)
        map[QString::number(c.key)] = QString::asprintf("#%02x%02x%02x", c.rgb[0], c.rgb[1], c.rgb[2]);
    m_colors = map;
    emit colorsChanged();
}
