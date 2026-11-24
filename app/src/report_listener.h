// SPDX-License-Identifier: GPL-3.0-or-later
// Listens to the keyboard's unsolicited input reports between (and during) the app's sessions,
// with the vendor hidraw node opened read-only: nothing is ever sent, so it needs no session
// lock and can't disturb another tool. For now only the brightness report (`fe 0b`, after an
// Fn brightness shortcut) is used.
#pragma once

#include <QObject>
#include <QString>

class QSocketNotifier;

class ReportListener : public QObject
{
    Q_OBJECT
public:
    explicit ReportListener(QObject *parent = nullptr);
    ~ReportListener() override;

    bool listening() const { return m_fd >= 0; }

    // Opens the keyboard's vendor node if not open yet; quietly does nothing when it isn't there.
    void open();
    // Listens on an already open descriptor (taken over); tests use a pipe.
    void listen(int fd);

signals:
    void brightnessReported(int value);

private:
    void readReports();
    void close();

    int m_fd = -1;
    QSocketNotifier *m_notifier = nullptr;
};
