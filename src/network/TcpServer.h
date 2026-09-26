#include <optional>
#pragma once

using namespace std;

class TcpServer
{
public:
    TcpServer(int port);
    bool start();
    int accept_client();
    int fd();
    void stop();
    pair<int, string> receive_message(int);
    bool send_message(int, string_view);
    bool close_client(int);

private:
    int port_;
    int server_socket_;
};