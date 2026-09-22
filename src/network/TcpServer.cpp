#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <optional>
#include "TcpServer.h"

using namespace std;

TcpServer::TcpServer(int port) : port_(port) {}

bool TcpServer::start()
{
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == -1)
        return false;

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        return false;

    if (bind(server_socket_, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
        return false;

    if (listen(server_socket_, SOMAXCONN) == -1)
        return false;

    return true;
}

bool TcpServer::accept_client()
{
    return accept(server_socket_, nullptr, nullptr) != -1;
}

optional<string> TcpServer::receive_message(int client_id)
{
    char buffer[1024];

    ssize_t bytes = recv(client_id, buffer, sizeof(buffer), 0);

    if (bytes <= 0)
        return nullopt;

    return string(buffer, bytes);
}

bool TcpServer::send_message(int client_id, string_view msg)
{
    size_t total_sent = 0;

    while (total_sent < msg.size())
    {
        ssize_t sent = send(client_id, msg.data() + total_sent, msg.size() - total_sent, 0);
        if (sent <= 0)
            return false;
        total_sent += sent;
    }

    return true;
}

void TcpServer::stop()
{
    close(server_socket_);
}
