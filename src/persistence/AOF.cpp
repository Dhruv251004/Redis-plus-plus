#include <iostream>
#include <fcntl.h>  // For open, O_WRONLY, O_CREAT, etc.
#include <unistd.h> // For close
#include "AOF.h"

using namespace std;

AOF::AOF(const string &path) : fd_(-1), path_(path) {}

bool AOF::open()
{
    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ == -1)
    {
        cerr << "Failed to open file with path : " << path_ << endl;
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
        cerr << "Failedto close file with fd : " << fd_ << endl;
    fd_ = -1;
}

bool AOF::append(string_view command)
{
    if (fd_ == -1)
        return false;

    ssize_t written = ::write(fd_, command.data(), command.size());
    return written == static_cast<ssize_t>(command.size());
}

bool AOF::fsync()
{
    if (fd_ == -1)
        return false;
    if (::fsync(fd_) == -1)
    {
        cerr << "failed to fsync file\n";
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
        cerr << "Failed to truncate aof file\n";
        return false;
    }
    return true;
}