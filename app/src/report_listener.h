// SPDX-License-Identifier: GPL-3.0-or-later
// Listens to the keyboard's unsolicited input reports between (and during) the app's sessions,
// with the vendor hidraw node opened read-only: nothing is ever sent, so it needs no session
// lock and can't disturb another tool. Used: the reports the board sends after an Fn shortcut
// changes the lighting (`fe 0b`) or the profile (`fe 09`).
#pragma once

#include "lights.h"

#include <QObject>
#include <QString>

#include <functional>

class QSocketNotifier;

class ReportListener : public QObject
{
    Q_OBJECT
public:
    explicit ReportListener(QObject *parent = nullptr);
    ~ReportListener() override;

    bool listening() const { return m_fd >= 0; }

    // Opens the keyboard's vendor node if not open yet; quietly does nothing when it isn't there.
    // Cheap when already listening: the Backend calls it after every read of the board.
    void open();
    // Listens on an already open descriptor (taken over); tests use a pipe.
    void listen(int fd);
    // What open() opens: a read-only descriptor, or -1. Default: the vendor hidraw node. Tests
    // pass pipes.
    void setOpener(std::function<int()> opener) { m_opener = std::move(opener); }

signals:
    void brightnessReported(int value);  // the last of a burst only
    // Any other change the board reported (effect, speed, colour, profile), in order.
    void boardReported(const akko::BoardReport &report);
    // The node stopped answering (unplugged, or the board dropped off USB and came back as a new
    // node): listening stopped until the next open().
    void lost();

private:
    void readReports();
    void close();

    int m_fd = -1;
    QSocketNotifier *m_notifier = nullptr;
    std::function<int()> m_opener;
};
