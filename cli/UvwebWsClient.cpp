
#include "WsClientOptions.h"
#include <iostream>
#include <uvweb/WebSocketClient.h>

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
            webSocketClient.sendText("Hello world");
        }
    });

    return 0;
}
