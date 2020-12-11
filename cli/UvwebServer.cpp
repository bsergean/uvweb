
#include "ServerOptions.h"
#include <uvw.hpp>
#include <uvweb/HttpServer.h>

class DemoHttpServer : public uvweb::HttpServer
{
public:
    DemoHttpServer(const std::string& host, int port)
        : uvweb::HttpServer(host, port)
    {
        ;
    }

    void processRequest(std::shared_ptr<uvweb::Request> request, uvweb::Response& response) final
    {
        response.statusCode = 200;
        response.description = "OK";
        response.body = "OK";
    }
};

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    DemoHttpServer httpServer(args.host, args.port);
    httpServer.run();

    auto loop = uvw::Loop::getDefault();
    loop->run();

    return 0;
}
