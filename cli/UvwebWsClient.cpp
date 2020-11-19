
#include "WsClientOptions.h"
#include <uvweb/WebSocketClient.h>
#include <iostream>

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.connect(
        args.url, 
        [](const uvweb::WebSocketMessagePtr& msg)
        {
            if (msg->type == uvweb::WebSocketMessageType::Message)
            {
                std::cout << "received message: " << msg->str << std::endl;
            }
            else if (msg->type == uvweb::WebSocketMessageType::Open)
            {
                std::cout << "Connection established" << std::endl;
            }
        }
    );

    return 0;
}
