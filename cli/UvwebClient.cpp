
#include "ClientOptions.h"
#include <iostream>
#include <uvweb/HttpClient.h>

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    uvweb::HttpClient httpClient;
    for (const auto& url : args.urls)
    {
        httpClient.fetch(url, [](std::shared_ptr<uvweb::Response> response) {
            std::cout << response->body << std::endl;
        });
    }

    auto loop = uvw::Loop::getDefault();
    loop->run();

    return 0;
}
