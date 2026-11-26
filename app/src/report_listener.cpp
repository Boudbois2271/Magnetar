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
    const int fd = m_opener ? m_opener() : [] {
        try {
            const std::string path = akko::findHidraw();
            return ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        } catch (const akko::Error &) {
            return -1;  // not plugged in: the next read of the board tries again
        }
    }();
    if (fd >= 0)
        listen(fd);
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
            if (last >= 0)
                emit brightnessReported(last);
            emit lost();
            return;
        }
        const auto r = akko::boardReport(std::span(buf.data(), std::size_t(n)));
        if (!r)
            continue;
        if (r->kind == akko::BoardReport::Kind::Brightness)
            last = r->value;
        else
            emit boardReported(*r);
    }
    if (last >= 0)
        emit brightnessReported(last);
}
