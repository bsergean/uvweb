
#include "options.h"
#include <uvweb/HttpServer.h>

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    uvweb::HttpServer httpServer(args.host, args.port);
    httpServer.run();

    return 0;
}
