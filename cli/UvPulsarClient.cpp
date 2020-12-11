
//
// https://pulsar.apache.org/docs/en/client-libraries-cpp/
//

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

    int receivedMessages = 0;

    if (args.subscribe)
    {
        pulsarClient.subscribe(
            args.tenant,
            args.nameSpace,
            args.topics[0],
            args.subscription,
            [&receivedMessages](const std::string& msg, const std::string& messageId) -> bool {
                std::cout << "Got message"
                          << " #" << receivedMessages << " value: " << msg
                          << " message id: " << messageId << std::endl;

                receivedMessages++;
                return true;
            });

        timer->on<uvw::TimerEvent>([&args, &receivedMessages, &pulsarClient](const auto&,
                                                                             auto& handle) {
            if (args.maxMessages == receivedMessages)
            {
                std::cout << "Received " << receivedMessages << " messages, exiting." << std::endl;
                handle.close();
                pulsarClient.close();
                return;
            }
        });

        timer->start(uvw::TimerHandle::Time {0}, uvw::TimerHandle::Time {100});
    }
    else
    {
        timer->on<uvw::TimerEvent>([&args, &pulsarClient](const auto&, auto& handle) {
            if (args.messages.empty())
            {
                if (pulsarClient.allPublishedMessagesProcessed())
                {
                    handle.close();
                    pulsarClient.close();
                }
                return;
            }

            // Grab the first message to send and pop it.
            auto message = args.messages.front();
            args.messages.erase(args.messages.begin()); // like pop_front()

            auto topic = args.topics.front();
            args.topics.erase(args.topics.begin()); // like pop_front()

            pulsarClient.publish(
                message,
                args.tenant,
                args.nameSpace,
                topic,
                [](bool success, const std::string& context, const std::string& messageId) {
                    std::string successString = (success) ? "true" : "false";
                    std::cout << "Publish successful: " << successString << " context " << context
                              << " message id: " << messageId << std::endl;
                });
        });

        timer->start(uvw::TimerHandle::Time {0}, uvw::TimerHandle::Time {args.delay});
    }

    loop->run();

    pulsarClient.reportStats();
    std::cout << "Loop terminated, exiting." << std::endl;
    return 0;
}
