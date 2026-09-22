#include <optional>
#pragma once

using namespace std;

class TcpServer
{
public:
    TcpServer(int port);
    bool start();
    bool accept_client();
    void stop();
    optional<string> receive_message(int);
    bool send_message(int, string_view);

private:
    int port_;
    int server_socket_;
};