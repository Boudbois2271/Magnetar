// SPDX-License-Identifier: GPL-3.0-or-later
// Tells the app when the keyboard's hidraw node appears, so plugging it in is noticed at once
// instead of on the next retry. Listens to udev (libudev), which announces a node after its
// rules ran, so the seat user's access is already granted by then.
#pragma once

#include <QObject>

class QSocketNotifier;
struct udev;
struct udev_monitor;

class HotplugWatch : public QObject
{
    Q_OBJECT
public:
    explicit HotplugWatch(QObject *parent = nullptr);
    ~HotplugWatch() override;

    // False when udev couldn't be reached (no netlink, a container...): the app then relies on
    // its retry timer alone.
    bool active() const { return m_notifier != nullptr; }

    // The USB vendor and product ids (sysfs text, "38ee" / "0016") are the keyboard's.
    static bool isKeyboard(const char *vendor, const char *product);

signals:
    void keyboardAdded();

private:
    void readEvents();

    udev *m_udev = nullptr;
    udev_monitor *m_monitor = nullptr;
    QSocketNotifier *m_notifier = nullptr;
};
