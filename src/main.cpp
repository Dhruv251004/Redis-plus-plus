#include <csignal>
#include <iostream>
#include <sys/socket.h>
#include <spdlog/spdlog.h>
#include "server/RedisServer.h"

using namespace std;

int main()
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    // Writing to a closed peer must surface as EPIPE, not kill the process.
    signal(SIGPIPE, SIG_IGN);
    RedisServer *server = new RedisServer(6379);
    server->start();
    return 0;
}
