// SPDX-License-Identifier: GPL-3.0-or-later
// Tells the app when the keyboard's hidraw nodes appear or go, so plugging it in or out is
// noticed at once instead of on the next retry or read. Listens to udev (libudev), which announces a node after its
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
    // A hidraw devpath (".../0003:38EE:0016.0007/hidraw/hidraw2") is one of the keyboard's. Used
    // for removals: by then the parent USB device's attributes can't be read any more.
    static bool isKeyboardDevpath(const char *devpath);

signals:
    void keyboardAdded();
    void keyboardRemoved(const QString &node);  // e.g. "/dev/hidraw2"

private:
    void readEvents();

    udev *m_udev = nullptr;
    udev_monitor *m_monitor = nullptr;
    QSocketNotifier *m_notifier = nullptr;
};
