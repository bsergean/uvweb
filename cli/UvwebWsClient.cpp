
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
void autobahn(Args& args);
void shell(Args& args);

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

    if (args.autobahn)
    {
        autobahn(args);
        return 0;
    }

    if (args.shell)
    {
        shell(args);
        return 0;
    }

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.setOnMessageCallback([&webSocketClient](const uvweb::WebSocketMessagePtr& msg) {
        if (msg->type == uvweb::WebSocketMessageType::Message)
        {
            std::cout << "received message: " << msg->str << std::endl;

            webSocketClient.close();
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
    webSocketClient.connect(args.url);

    auto loop = uvw::Loop::getDefault();
    loop->run();

    return 0;
}

void autoroute(Args& args)
{
    std::atomic<uint64_t> receivedCountPerSecs(0);
    std::atomic<bool> stop(false);

    uint64_t target(args.msgCount);
    std::chrono::time_point<std::chrono::high_resolution_clock> start;

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.setOnMessageCallback([&receivedCountPerSecs, &webSocketClient, &target, &start](const uvweb::WebSocketMessagePtr& msg) {
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

    std::string fullUrl(args.url);
    fullUrl += "/";
    fullUrl += std::to_string(args.msgCount);
    webSocketClient.connect(fullUrl);

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

void autobahn(Args& args)
{
    int testCasesCount = -1;

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.setOnMessageCallback([&testCasesCount, &webSocketClient](const uvweb::WebSocketMessagePtr& msg) {
        if (msg->type == uvweb::WebSocketMessageType::Message)
        {
            // response is a string
            std::stringstream ss;
            ss << msg->str;
            ss >> testCasesCount;

            webSocketClient.close(); // ??
        }
        else if (msg->type == uvweb::WebSocketMessageType::Open)
        {
            spdlog::info("uvweb autobahn: connected");
            spdlog::info("Uri: {}", msg->openInfo.uri);
            spdlog::info("Headers:");
            for (auto it : msg->openInfo.headers)
            {
                spdlog::info("{}: {}", it.first, it.second);
            }
        }
    });
    std::string caseCountUrl(args.url);
    caseCountUrl += "/getCaseCount";
    webSocketClient.connect(caseCountUrl);

    auto loop = uvw::Loop::getDefault();
    loop->run();

    std::cout << "Case count: " << testCasesCount << std::endl;

    if (testCasesCount == -1)
    {
        spdlog::error("Cannot retrieve test case count at url {}", args.url);
        return;
    }

    testCasesCount++;

    for (int i = 1; i < testCasesCount; ++i)
    {
        spdlog::info("Execute test case {}", i);

        int caseNumber = i;

        std::stringstream ss;
        ss << args.url << "/runCase?case=" << caseNumber << "&agent=uvweb";

        std::string url(ss.str());

        uvweb::WebSocketClient wsClient;
        wsClient.setOnMessageCallback([&testCasesCount, &wsClient](const uvweb::WebSocketMessagePtr& msg) {
            if (msg->type == uvweb::WebSocketMessageType::Message)
            {
                wsClient.send(msg->str, msg->binary);
            }
            else if (msg->type == uvweb::WebSocketMessageType::Open)
            {
                spdlog::info("uvweb autobahn: connected");
                spdlog::info("Uri: {}", msg->openInfo.uri);
                spdlog::info("Headers:");
                for (auto it : msg->openInfo.headers)
                {
                    spdlog::info("{}: {}", it.first, it.second);
                }
            }
        });
        wsClient.connect(url);

        loop->run();
    }

    spdlog::info("Generate report");

    std::stringstream ss;
    ss << args.url << "/updateReports?agent=uvweb";

    webSocketClient.setOnMessageCallback([&webSocketClient](const uvweb::WebSocketMessagePtr& msg) {
        if (msg->type == uvweb::WebSocketMessageType::Message)
        {
            spdlog::info("Report generated");
        }
    });
    webSocketClient.connect(ss.str());

    loop->run();
}

// 
// Using a timer ...
//
// auto timer = loop->resource<uvw::TimerHandle>();
// timer->on<uvw::TimerEvent>([handle](const auto &, auto &hndl){
//     // auto data = std::make_unique<char[]>('*');
//     // handle->write(std::move(data), 1);
//     // hndl.close();
// });
// timer->start(uvw::TimerHandle::Time{0}, uvw::TimerHandle::Time{1000});
//     

void shell(Args& args)
{
    auto loop = uvw::Loop::getDefault();

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.setOnMessageCallback([&webSocketClient](const uvweb::WebSocketMessagePtr& msg) {
        if (msg->type == uvweb::WebSocketMessageType::Message)
        {
            std::cout << "received message: " << msg->str << std::endl;
            // webSocketClient.close();
        }
        else if (msg->type == uvweb::WebSocketMessageType::Open)
        {
            std::cout << "Connection established" << std::endl;
        }
        else if (msg->type == uvweb::WebSocketMessageType::Close)
        {
            std::cout << "Connection closed" << std::endl;
        }
    });

    webSocketClient.connect(args.url);

    // Retrieve typed data
    auto handle = loop->resource<uvw::TTYHandle>(uvw::StdIN, true);
    if (!handle->init())
    {
        spdlog::info("cannot init handle");
        return;
    }
    if (!handle->mode(uvw::TTYHandle::Mode::NORMAL))
    {
        spdlog::info("cannot set mode");
        return;
    }

    handle->on<uvw::DataEvent>([&webSocketClient](const auto & event, auto &hndl){
        std::string msg(event.data.get(), event.length);

        if (msg == "/close\n")
        {
            webSocketClient.close();
        }
        else
        {
            if (!webSocketClient.sendText(msg))
            {
                // if the connection is closed sending should fail
                std::cerr << "Error sending text" << std::endl;
            }
        }
    });

    handle->read();
    loop->run();
}
