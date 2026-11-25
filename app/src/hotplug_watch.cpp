// SPDX-License-Identifier: GPL-3.0-or-later
#include "hotplug_watch.h"

#include <QSocketNotifier>
#include <QStringList>

#include <cstring>

#include <libudev.h>

HotplugWatch::HotplugWatch(QObject *parent) : QObject(parent)
{
    m_udev = udev_new();
    if (!m_udev)
        return;
    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor || udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "hidraw", nullptr) < 0
        || udev_monitor_enable_receiving(m_monitor) < 0)
        return;
    m_notifier = new QSocketNotifier(udev_monitor_get_fd(m_monitor), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &HotplugWatch::readEvents);
}

HotplugWatch::~HotplugWatch()
{
    delete m_notifier;
    if (m_monitor)
        udev_monitor_unref(m_monitor);
    if (m_udev)
        udev_unref(m_udev);
}

bool HotplugWatch::isKeyboard(const char *vendor, const char *product)
{
    return vendor && product && std::strcmp(vendor, "38ee") == 0 && std::strcmp(product, "0016") == 0;
}

bool HotplugWatch::isKeyboardDevpath(const char *devpath)
{
    // The HID device's directory is named bus:vendor:product.instance, in upper-case hex.
    return devpath && std::strstr(devpath, ":38EE:0016.") != nullptr;
}

void HotplugWatch::readEvents()
{
    bool added = false;
    QStringList removed;
    // The socket is non-blocking: drain every queued event, then signal.
    while (udev_device *dev = udev_monitor_receive_device(m_monitor)) {
        const char *action = udev_device_get_action(dev);
        if (action && std::strcmp(action, "add") == 0) {
            udev_device *usb = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
            if (usb && isKeyboard(udev_device_get_sysattr_value(usb, "idVendor"),
                                  udev_device_get_sysattr_value(usb, "idProduct")))
                added = true;
        } else if (action && std::strcmp(action, "remove") == 0
                   && isKeyboardDevpath(udev_device_get_devpath(dev))) {
            if (const char *node = udev_device_get_devnode(dev))
                removed << QString::fromUtf8(node);
        }
        udev_device_unref(dev);  // also frees the parent, which it owns
    }
    for (const QString &node : removed)
        emit keyboardRemoved(node);
    if (added)
        emit keyboardAdded();
}
