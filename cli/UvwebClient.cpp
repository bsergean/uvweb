
#include "ClientOptions.h"
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
        httpClient.fetch(url);
    }

    return 0;
}
