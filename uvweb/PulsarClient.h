
#pragma once

#include "WebSocketClient.h"

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <memory>
#include <functional>

namespace uvweb
{
    using OnResponseCallback = std::function<void(bool, const std::string&)>;

    class PulsarClient
    {
    public:
        PulsarClient(const std::string& baseUrl);

        void send(
            const std::string& str,
            const std::string& tenant,
            const std::string& nameSpace,
            const std::string& topic,
            const OnResponseCallback& callback);

    private:
        std::pair<bool, std::shared_ptr<WebSocketClient>> getWebSocketClient(const std::string& key);
        void publish(const std::string& str, std::shared_ptr<WebSocketClient>);
        void processReceivedMessage(const std::string& str);
        std::string createContext();
        
        std::string serializePublishMessage(
            const std::string& str,
            const std::string& context);

        void createQueueProcessor();

        std::map<std::string, std::shared_ptr<WebSocketClient>> _clients;
        std::map<std::string, OnResponseCallback> _callbacks;
        std::string _baseUrl;
        uint64_t _mId;

        // We could have multiple queues per topic
        std::queue<std::pair<std::string, std::string>> _queue;

        std::shared_ptr<uvw::TimerHandle> _timer;
    };
}
