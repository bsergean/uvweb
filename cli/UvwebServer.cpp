
#include "ServerOptions.h"
#include <uvweb/HttpServer.h>
#include <uvw.hpp>

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    uvweb::HttpServer httpServer(args.host, args.port);
    httpServer.run();

    auto loop = uvw::Loop::getDefault();
    loop->run();

    return 0;
}
