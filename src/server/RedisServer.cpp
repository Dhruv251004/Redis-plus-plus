#include <iostream>
#include "RedisServer.h"
#include "protocol/RespParser.h"
#include "command/CommandExecutor.h"
#include "protocol/RespSerializer.h"

using namespace std;

RedisServer::RedisServer(int port) : tcp_server_(port) {}

void RedisServer::start()
{
    bool res = tcp_server_.start();
    if (!res)
        return;

    while (true)
    {
        int client_id = tcp_server_.accept_client();
        if (client_id != -1)
        {
            handle_client(client_id);
            if (tcp_server_.close_client(client_id))
            {
                cout << "client disconnected\n";
            }
            else
            {
                cout << "failed to close connection for client id " << client_id << endl;
            }
        }
        else
        {
            cout << "failed to accept client connection" << endl;
        }
    }
}

void RedisServer::handle_client(int client_id)
{
    while (true)
    {
        auto msg = tcp_server_.receive_message(client_id);
        if (!msg.has_value())
        {
            break;
        }

        RespValue input_resp = RespParser::parse(*msg);
        RespValue output_resp = CommandExecutor::execute(input_resp);

        string output_msg = RespSerializer::serialize(output_resp);
        if (!tcp_server_.send_message(client_id, output_msg))
        {
            break;
        }
    }
}

void RedisServer::stop()
{
    tcp_server_.stop();
}
