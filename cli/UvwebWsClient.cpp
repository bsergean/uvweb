
#include "WsClientOptions.h"
#include <iostream>
#include <uvweb/WebSocketClient.h>
#include <uvw.hpp>

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.connect(args.url, [&webSocketClient](const uvweb::WebSocketMessagePtr& msg) {
        if (msg->type == uvweb::WebSocketMessageType::Message)
        {
            std::cout << "received message: " << msg->str << std::endl;
        }
        else if (msg->type == uvweb::WebSocketMessageType::Open)
        {
            std::cout << "Connection established" << std::endl;
            if (!webSocketClient.sendText("Hello world"))
            {
                std::cerr << "Error sending text" << std::endl;
            }
        }
    });

    auto loop = uvw::Loop::getDefault();
    loop->run();

    return 0;
}
