
#include "PulsarOptions.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <uvw.hpp>
#include <uvweb/PulsarClient.h>

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    uvweb::PulsarClient pulsarClient(args.url);

    pulsarClient.send(
        args.msg,
        args.tenant,
        args.nameSpace,
        args.topic,
        [](bool success) {
            std::cout << "Publish successful: " << success << std::endl;
        }
    );

    auto loop = uvw::Loop::getDefault();
    loop->run();

    return 0;
}
