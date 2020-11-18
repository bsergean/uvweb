
#include "WsClientOptions.h"
#include <uvweb/WebSocketClient.h>

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.connect(args.url);

    return 0;
}
