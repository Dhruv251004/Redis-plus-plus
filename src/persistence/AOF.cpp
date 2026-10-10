#include <iostream>
#include <fcntl.h>  // For open, O_WRONLY, O_CREAT, etc.
#include <unistd.h> // For close
#include <cstring>
#include <spdlog/spdlog.h>
#include "AOF.h"

using namespace std;

AOF::AOF(const string &path) : fd_(-1), path_(path) {}

bool AOF::open()
{
    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ == -1)
    {
        spdlog::error("failed to open aof file {}: {}", path_, strerror(errno));
        return false;
    }
    return true;
}

void AOF::close()
{
    if (fd_ == -1)
        return;

    int res = ::close(fd_);
    if (res == -1)
        spdlog::error("failed to close aof fd {}: {}", fd_, strerror(errno));
    fd_ = -1;
}

bool AOF::append(string_view command)
{
    if (fd_ == -1)
        return false;

    ssize_t written = ::write(fd_, command.data(), command.size());
    if (written != static_cast<ssize_t>(command.size()))
    {
        spdlog::error("failed to write to aof file: {}", written < 0 ? strerror(errno) : "short write");
        return false;
    }
    return true;
}

bool AOF::fsync()
{
    if (fd_ == -1)
        return false;
    if (::fsync(fd_) == -1)
    {
        spdlog::error("failed to fsync aof file: {}", strerror(errno));
        return false;
    }
    return true;
}

bool AOF::truncate()
{
    if (fd_ == -1)
        return false;

    if (ftruncate(fd_, 0) == -1)
    {
        spdlog::error("failed to truncate aof file: {}", strerror(errno));
        return false;
    }
    return true;
}