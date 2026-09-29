#include <iostream>
#include <fcntl.h>  // For open, O_WRONLY, O_CREAT, etc.
#include <unistd.h> // For close
#include <iostream>
#include "AOF.h"

using namespace std;

AOF::AOF(const string &path) : path_(path) {}

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
    int res = ::close(fd_);
    if (res == -1)
        cerr << "Failedto close file with fd : " << fd_ << endl;
}

bool AOF::append(string command)
{
    ssize_t written = ::write(fd_, command.data(), command.size());

    return written == static_cast<ssize_t>(command.size());
}