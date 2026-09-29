#pragma once
#include <string>
#include <unistd.h> // read(), close()
#include <fcntl.h>  // open()

class AOF
{
public:
    AOF(const std::string &);

    bool open();
    bool append(std::string);
    void close();

    template <typename F>
    void replay(F callback)
    {
        std::string content;
        char buffer[4096]; // A 4KB chunks buffer
        ssize_t bytes_read;

        while ((bytes_read = ::read(fd_, buffer, sizeof(buffer))) > 0)
        {
            content.append(buffer, bytes_read);
        }

        callback(content);
    }

private:
    int fd_;
    std::string path_;
};