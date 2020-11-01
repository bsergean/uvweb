
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
    httpClient.fetch(args.url);

    return 0;
}
