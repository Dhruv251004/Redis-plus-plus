#include <iostream>
#include <sys/socket.h>
#include "network/TcpServer.h"

using namespace std;

int main()
{
    TcpServer server(6379);
    server.start();
    int client_socket = server.accept_client();

    char buffer[1024] = {0};
    recv(client_socket, buffer, sizeof(buffer), 0);
    cout << "Message from client: " << buffer << endl;
    return 0;
}
