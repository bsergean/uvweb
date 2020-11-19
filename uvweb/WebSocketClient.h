
#pragma once

#include "WebSocketMessage.h"

#include <string>
#include <map>
#include <functional>

namespace uvweb
{
    struct Request
    {
        WebSocketHttpHeaders headers;
        std::string currentHeaderName;
        std::string currentHeaderValue;
        std::string path;
        std::string method;
        std::string host;
        bool messageComplete = false;
    };

    struct Response
    {
        WebSocketHttpHeaders headers;
        int statusCode = 0;
        std::string description;
        std::string body;
        std::string uri; // FIXME missing
        std::string protocol; // FIXME missing subprotocol ??

        std::string currentHeaderName;
        std::string currentHeaderValue;
        bool messageComplete = false;
    };

    enum class ReadyState
    {
        Connecting = 0,
        Open = 1,
        Closing = 2,
        Closed = 3
    };

    using OnMessageCallback = std::function<void(const WebSocketMessagePtr&)>;

    class WebSocketClient
    {
    public:
        WebSocketClient();
        void connect(const std::string& url,
                     const OnMessageCallback& callback);

    private:
    };
}
