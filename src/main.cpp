#include <iostream>
#include <sys/socket.h>
#include "server/RedisServer.h"

using namespace std;

int main()
{
    RedisServer *server = new RedisServer(6379);
    server->start();
    return 0;
}
