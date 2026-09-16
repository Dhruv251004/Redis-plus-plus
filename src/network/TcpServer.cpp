#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "TcpServer.h"

using namespace std;

TcpServer::TcpServer(int port) : port_(port) {}

void TcpServer::start()
{
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_);
    server_address.sin_addr.s_addr = INADDR_ANY;
    bind(server_socket_, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(server_socket_, SOMAXCONN);
}

int TcpServer::accept_client()
{
    return accept(server_socket_, nullptr, nullptr);
}

void TcpServer::stop()
{
    close(server_socket_);
}