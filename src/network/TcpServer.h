#pragma once

class TcpServer
{
public:
    TcpServer(int port);
    void start();
    int accept_client();
    void stop();

private:
    int port_;
    int server_socket_;
};