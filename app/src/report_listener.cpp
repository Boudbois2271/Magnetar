// SPDX-License-Identifier: GPL-3.0-or-later
#include "report_listener.h"

#include "device.h"
#include "lights.h"

#include <QSocketNotifier>

#include <cerrno>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

ReportListener::ReportListener(QObject *parent) : QObject(parent) {}

ReportListener::~ReportListener()
{
    close();
}

void ReportListener::open()
{
    if (listening())
        return;
    try {
        const std::string path = akko::findHidraw();
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0)
            listen(fd);
    } catch (const akko::Error &) {
        // Not plugged in: the next connection or hotplug event tries again.
    }
}

void ReportListener::listen(int fd)
{
    close();
    m_fd = fd;
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &ReportListener::readReports);
}

void ReportListener::close()
{
    delete m_notifier;
    m_notifier = nullptr;
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
}

void ReportListener::readReports()
{
    std::vector<std::uint8_t> buf(akko::kReportLen);
    int last = -1;
    for (;;) {
        const ssize_t n = ::read(m_fd, buf.data(), buf.size());
        if (n < 0 && (errno == EAGAIN || errno == EINTR))
            break;
        if (n <= 0) {  // unplugged (or end of a test pipe): stop until the next open()
            close();
            break;
        }
        if (const auto v = akko::brightnessReport(std::span(buf.data(), std::size_t(n))))
            last = *v;
    }
    if (last >= 0)
        emit brightnessReported(last);
}
