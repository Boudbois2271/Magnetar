// SPDX-License-Identifier: GPL-3.0-or-later
#include "device.h"
#include "frame.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace akko {

namespace {

bool readFile(const fs::path &path, std::string &out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return !in.bad();
}

}  // namespace

std::string findHidraw(const std::string &sysRoot)
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(sysRoot, ec))
        names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());  // same order as the Python tool: hidraw10 before hidraw2

    char id[24];
    std::snprintf(id, sizeof id, "%08X:%08X", kVendorId, kProductId);
    const std::string usagePage("\x06\x60\xff", 3);  // Usage Page (Vendor 0xFF60)

    for (const auto &name : names) {
        const fs::path dev = fs::path(sysRoot) / name / "device";
        std::string uevent, rdesc;
        if (!readFile(dev / "uevent", uevent) || !readFile(dev / "report_descriptor", rdesc))
            continue;
        if (uevent.find(id) != std::string::npos && rdesc.find(usagePage) != std::string::npos)
            return "/dev/" + name;
    }
    throw Error("keyboard not found (is it plugged in?)");
}

std::string defaultLockPath()
{
    const char *dir = std::getenv("XDG_RUNTIME_DIR");
    return std::string(dir ? dir : "/tmp") + "/akkoctl.lock";
}

SessionLock::SessionLock(const std::string &path, int timeoutMs)
{
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd_ < 0)
        throw Error("cannot open lock " + path + ": " + std::strerror(errno));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        if (errno != EWOULDBLOCK && errno != EINTR) {
            const int err = errno;
            ::close(fd_);
            throw Error("cannot lock " + path + ": " + std::strerror(err));
        }
        if (std::chrono::steady_clock::now() > deadline) {
            ::close(fd_);
            throw Error("keyboard busy (another akkoctl is running)");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

SessionLock::~SessionLock()
{
    ::close(fd_);  // closing the descriptor releases the flock
}

HidrawTransport::HidrawTransport(const std::string &path)
{
    fd_ = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        if (errno == EACCES || errno == EPERM)
            throw Error("no permission for " + path + " (udev rule missing?)");
        throw Error("cannot open " + path + ": " + std::strerror(errno));
    }
}

HidrawTransport::~HidrawTransport()
{
    ::close(fd_);
}

void HidrawTransport::drain()
{
    std::uint8_t buf[kReportLen];
    pollfd p{fd_, POLLIN, 0};
    while (::poll(&p, 1, 0) > 0 && (p.revents & POLLIN)) {
        if (::read(fd_, buf, sizeof buf) < 0)
            break;
    }
}

void HidrawTransport::write(std::span<const std::uint8_t> report)
{
    const ssize_t n = ::write(fd_, report.data(), report.size());
    if (n < 0)
        throw Error(std::string("write to keyboard failed: ") + std::strerror(errno));
    if (static_cast<std::size_t>(n) != report.size())
        throw Error("short write to keyboard");
}

std::optional<std::vector<std::uint8_t>> HidrawTransport::read(int timeoutMs)
{
    pollfd p{fd_, POLLIN, 0};
    const int r = ::poll(&p, 1, std::max(0, timeoutMs));
    if (r < 0 && errno == EINTR)
        return std::vector<std::uint8_t>{};  // not a timeout: let the caller check its deadline
    if (r <= 0)
        return std::nullopt;
    if (p.revents & (POLLERR | POLLHUP | POLLNVAL))
        throw Error("keyboard disconnected");
    std::vector<std::uint8_t> buf(kReportLen);
    const ssize_t n = ::read(fd_, buf.data(), buf.size());
    if (n < 0) {
        if (errno == EAGAIN || errno == EINTR)
            return std::vector<std::uint8_t>{};
        throw Error(std::string("read from keyboard failed: ") + std::strerror(errno));
    }
    buf.resize(static_cast<std::size_t>(n));
    return buf;
}

Connection::Connection()
    : lock_(), transport_(findHidraw())
{
}

}  // namespace akko
