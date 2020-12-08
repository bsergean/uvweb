
#include "WsClientOptions.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <uvw.hpp>
#include <uvweb/WebSocketClient.h>

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

            SPDLOG_INFO("Uri: {}", msg->openInfo->uri);
            SPDLOG_INFO("Headers:");
            for (auto it : msg->openInfo->headers)
            {
                SPDLOG_INFO("{}: {}", it.first, it.second);
            }

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

class AutorouteWebSocketClient : public uvweb::WebSocketClient
{
public:
    uint64_t receivedCountPerSecs;
    std::shared_ptr<uvw::TimerHandle> timer;

    uint64_t target;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;

    virtual void invokeOnMessageCallback(const uvweb::WebSocketMessagePtr& msg) final
    {
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

                SPDLOG_INFO("AUTOROUTE uvweb :: {} ms", duration);

                close(); // ??
                timer->close();
            }
        }
        else if (msg->type == uvweb::WebSocketMessageType::Open)
        {
            createTimer();

            SPDLOG_INFO("uvweb autoroute: connected");
            SPDLOG_INFO("Uri: {}", msg->openInfo->uri);
            SPDLOG_INFO("Headers:");
            for (auto it : msg->openInfo->headers)
            {
                SPDLOG_INFO("{}: {}", it.first, it.second);
            }

            start = std::chrono::high_resolution_clock::now();
        }
    }

    void createTimer()
    {
        auto loop = uvw::Loop::getDefault();
        timer = loop->resource<uvw::TimerHandle>();
        timer->on<uvw::TimerEvent>([this](const auto&, auto& hndl) {
            std::stringstream ss;
            ss << "messages received per second: " << receivedCountPerSecs;

            SPDLOG_INFO(ss.str());
            receivedCountPerSecs = 0;
        });
        timer->start(uvw::TimerHandle::Time {0}, uvw::TimerHandle::Time {1000});
    }
};

void autoroute(Args& args)
{
    AutorouteWebSocketClient webSocketClient;
    webSocketClient.target = args.msgCount;
    webSocketClient.receivedCountPerSecs = 0;

    std::string fullUrl(args.url);
    fullUrl += "/";
    fullUrl += std::to_string(args.msgCount);
    webSocketClient.connect(fullUrl);

    auto loop = uvw::Loop::getDefault();
    loop->run();
}

void autobahn(Args& args)
{
    int testCasesCount = -1;

    uvweb::WebSocketClient webSocketClient;
    webSocketClient.setOnMessageCallback(
        [&testCasesCount, &webSocketClient](const uvweb::WebSocketMessagePtr& msg) {
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
                SPDLOG_DEBUG("uvweb autobahn: connected");
                SPDLOG_DEBUG("Uri: {}", msg->openInfo->uri);
                SPDLOG_DEBUG("Headers:");
                for (auto it : msg->openInfo->headers)
                {
                    SPDLOG_DEBUG("{}: {}", it.first, it.second);
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
        SPDLOG_ERROR("Cannot retrieve test case count at url {}", args.url);
        return;
    }

    testCasesCount++;

    for (int i = 1; i < testCasesCount; ++i)
    {
        std::cout << "Execute test case " << i << std::endl;

        int caseNumber = i;

        std::stringstream ss;
        ss << args.url << "/runCase?case=" << caseNumber << "&agent=uvweb";

        std::string url(ss.str());

        uvweb::WebSocketClient wsClient;
        wsClient.setOnMessageCallback(
            [&testCasesCount, &wsClient](const uvweb::WebSocketMessagePtr& msg) {
                if (msg->type == uvweb::WebSocketMessageType::Message)
                {
                    wsClient.send(msg->str, msg->binary);
                }
                else if (msg->type == uvweb::WebSocketMessageType::Open)
                {
                    SPDLOG_DEBUG("uvweb autobahn: connected");
                    SPDLOG_DEBUG("Uri: {}", msg->openInfo->uri);
                    SPDLOG_DEBUG("Headers:");
                    for (auto it : msg->openInfo->headers)
                    {
                        SPDLOG_DEBUG("{}: {}", it.first, it.second);
                    }
                }
            });
        wsClient.connect(url);

        loop->run();
    }

    SPDLOG_INFO("Generate report");

    std::stringstream ss;
    ss << args.url << "/updateReports?agent=uvweb";

    webSocketClient.setOnMessageCallback([&webSocketClient](const uvweb::WebSocketMessagePtr& msg) {
        if (msg->type == uvweb::WebSocketMessageType::Message)
        {
            std::cout << "Report generated" << std::endl;
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
            std::cout << "Received message: " << msg->str << std::endl;
        }
        else if (msg->type == uvweb::WebSocketMessageType::Open)
        {
            std::cout << "Connection established" << std::endl;
        }
        else if (msg->type == uvweb::WebSocketMessageType::Close)
        {
            std::cout << "Connection closed. Type Ctrl^D to exit." << std::endl;
        }
    });

    webSocketClient.connect(args.url);

    // Retrieve typed data
    auto handle = loop->resource<uvw::TTYHandle>(uvw::StdIN, true);
    if (!handle->init())
    {
        SPDLOG_INFO("Cannot init handle");
        return;
    }
    if (!handle->mode(uvw::TTYHandle::Mode::NORMAL))
    {
        SPDLOG_INFO("Cannot set tty mode");
        return;
    }

    handle->on<uvw::DataEvent>([&webSocketClient, &args](const auto& event, auto& hndl) {
        std::string msg(event.data.get(), event.length);

        if (msg == "/close\n")
        {
            webSocketClient.close();
        }
        else if (msg == "/connect\n")
        {
            webSocketClient.connect(args.url);
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
