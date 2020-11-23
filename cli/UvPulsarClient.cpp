
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

    for (int i = 0; i < args.messages.size(); ++i)
    {
        pulsarClient.send(args.messages[i],
                          args.tenant,
                          args.nameSpace,
                          args.topics[i],
                          [](bool success, const std::string& messageId) {
                              std::cout << "Publish successful: " << success
                                        << " message id: " << messageId << std::endl;
                          });
    }

    auto loop = uvw::Loop::getDefault();
    loop->run();

    std::cout << "Loop terminated, exiting." << std::endl;
    return 0;
}
