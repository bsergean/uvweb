
#include "WsClientOptions.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <atomic>
#include <thread>
#include <uvweb/WebSocketClient.h>
#include <uvw.hpp>
#include <spdlog/spdlog.h>

void autoroute(Args& args);

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

    if (args.autoroute)
    {
        autoroute(args);
        return 0;
    }

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.connect(args.url, [&webSocketClient](const uvweb::WebSocketMessagePtr& msg) {
        if (msg->type == uvweb::WebSocketMessageType::Message)
        {
            std::cout << "received message: " << msg->str << std::endl;

            if (!webSocketClient.sendText("Hello world"))
            {
                std::cerr << "Error sending text" << std::endl;
            }
        }
        else if (msg->type == uvweb::WebSocketMessageType::Open)
        {
            std::cout << "Connection established" << std::endl;
            if (!webSocketClient.sendText("Hello world"))
            {
                std::cerr << "Error sending text" << std::endl;
            }
        }
    });

    auto loop = uvw::Loop::getDefault();
    loop->run();

    return 0;
}

void autoroute(Args& args)
{
    std::string fullUrl(args.url);
    fullUrl += "/";
    fullUrl += std::to_string(args.msgCount);

    std::atomic<uint64_t> receivedCountPerSecs(0);
    std::atomic<bool> stop(false);

    uint64_t target(args.msgCount);
    std::chrono::time_point<std::chrono::high_resolution_clock> start;

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.connect(fullUrl, [&receivedCountPerSecs, &webSocketClient, &target, &start](const uvweb::WebSocketMessagePtr& msg) {
        if (msg->type == uvweb::WebSocketMessageType::Message)
        {
            receivedCountPerSecs++;

            target -= 1;
            if (target == 0)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto milliseconds =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
                auto duration = milliseconds.count();

                spdlog::info("AUTOROUTE uvweb :: {} ms", duration);

                webSocketClient.close(); // ??
            }
        }
        else if (msg->type == uvweb::WebSocketMessageType::Open)
        {
            spdlog::info("uvweb autoroute: connected");
            spdlog::info("Uri: {}", msg->openInfo.uri);
            spdlog::info("Headers:");
            for (auto it : msg->openInfo.headers)
            {
                spdlog::info("{}: {}", it.first, it.second);
            }

            start = std::chrono::high_resolution_clock::now();
        }
    });

    auto timer = [&receivedCountPerSecs, &stop] {
        while (!stop)
        {
            //
            // We cannot write to sentCount and receivedCount
            // as those are used externally, so we need to introduce
            // our own counters
            //
            std::stringstream ss;
            ss << "messages received per second: " << receivedCountPerSecs;

            spdlog::info(ss.str());

            receivedCountPerSecs = 0;

            auto duration = std::chrono::seconds(1);
            std::this_thread::sleep_for(duration);
        }
    };

    std::thread t1(timer);

    auto loop = uvw::Loop::getDefault();
    loop->run();
}

