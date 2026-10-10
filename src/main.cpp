#include <iostream>
#include <sys/socket.h>
#include <spdlog/spdlog.h>
#include "server/RedisServer.h"

using namespace std;

int main()
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    RedisServer *server = new RedisServer(6379);
    server->start();
    return 0;
}
