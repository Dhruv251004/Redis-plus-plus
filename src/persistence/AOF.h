#pragma once
#include <cerrno>
#include <cstring>
#include <string>
#include <spdlog/spdlog.h>
#include <string_view>
#include <unistd.h> // read(), close()
#include <fcntl.h>  // open()
using namespace std;

class AOF
{
public:
    AOF(const std::string &);

    bool open();
    bool append(std::string_view);
    void close();
    bool fsync();
    bool truncate();

    template <typename F>
    void replay(F callback)
    {
        int read_fd = ::open(path_.c_str(), O_RDONLY);

        if (read_fd == -1)
        {
            if (errno != ENOENT)
                spdlog::error("[AOF.h::AOF::replay] failed to open aof file for replay {}: {}", path_, strerror(errno));
            return;
        }

        std::string content;
        char buffer[4096];
        ssize_t bytes_read;

        while ((bytes_read = ::read(read_fd, buffer, sizeof(buffer))) > 0)
        {
            content.append(buffer, bytes_read);
        }

        if (bytes_read < 0)
            spdlog::error("[AOF.h::AOF::replay] failed to read aof file {}: {}", path_, strerror(errno));

        ::close(read_fd);

        spdlog::info("[AOF.h::AOF::replay] replaying aof file, {} bytes", content.size());
        callback(content);
    }

private:
    int fd_ = -1;
    std::string path_;
};