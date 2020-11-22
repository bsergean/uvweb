
#pragma once

#include "WebSocketClient.h"

#include <string>
#include <map>
#include <vector>
#include <functional>

namespace uvweb
{
    using OnResponseCallback = std::function<void(bool)>;

    class PulsarClient
    {
    public:
        PulsarClient(const std::string& url);

        void send(
            const std::string& str,
            const std::string& tenant,
            const std::string& nameSpace,
            const std::string& topic,
            const OnResponseCallback& callback);

    private:
        std::map<std::string, WebSocketClient> _clients;
        std::string _url;
    };
}
