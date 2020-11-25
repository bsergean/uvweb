
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

    auto loop = uvw::Loop::getDefault();
    auto timer = loop->resource<uvw::TimerHandle>();

    uvweb::PulsarClient pulsarClient(args.url);

    if (args.subscribe)
    {
        pulsarClient.subscribe(args.tenant,
                               args.nameSpace,
                               args.topics[0],
                               args.subscription,
                               [](const std::string& msg, const std::string& messageId) -> bool {
                                   std::cout << "Got message " << msg
                                             << " message id: " << messageId << std::endl;
                                   return true;
                               });
    }
    else
    {
        timer->on<uvw::TimerEvent>([&args, &pulsarClient](const auto&, auto& handle) {
            if (args.messages.empty())
            {
                handle.close();
                pulsarClient.close();
                return;
            }

            // Grab the first message to send and pop it.
            auto message = args.messages.front();
            args.messages.erase( args.messages.begin() ); // like pop_front()

            auto topic = args.topics.front();
            args.topics.erase( args.topics.begin() ); // like pop_front()

            pulsarClient.publish(message,
                                 args.tenant,
                                 args.nameSpace,
                                 topic,
                                 [](bool success, const std::string& messageId) {
                                     std::cout << "Publish successful: " << success
                                               << " message id: " << messageId << std::endl;
                                 });
        });

        timer->start(uvw::TimerHandle::Time {0}, uvw::TimerHandle::Time {args.delay});
    }

    loop->run();

    std::cout << "Loop terminated, exiting." << std::endl;
    return 0;
}
