
#pragma once

#include "WebSocketClient.h"

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <memory>
#include <functional>
#include <list>

namespace uvweb
{
    using OnPublishResponseCallback = std::function<void(bool, // success
                                                         const std::string&, // context
                                                         const std::string&  // message id
                                                         )>;
    using OnSubscribeResponseCallback = std::function<bool(const std::string&,
                                                           const std::string&)>;

    class PulsarClient
    {
    public:
        PulsarClient(const std::string& baseUrl, size_t maxQueueSize = PulsarClient::_defaultMaxQueueSize);

        void publish(
            const std::string& str,
            const std::string& tenant,
            const std::string& nameSpace,
            const std::string& topic,
            const OnPublishResponseCallback& callback);

        void subscribe(
            const std::string& tenant,
            const std::string& nameSpace,
            const std::string& topic,
            const std::string& subscription,
            const OnSubscribeResponseCallback& callback);

        void close();

        void reportStats() const;

        // Tells whether the queue is empty, and all callbacks were invoked
        // whether the message delivery was successfull or not
        bool allPublishedMessagesProcessed() const;

    private:
        std::pair<bool, std::shared_ptr<WebSocketClient>> getWebSocketClient(const std::string& key);
        void publish(const std::string& str, std::shared_ptr<WebSocketClient>);

        void processProducerReceivedMessage(const std::string& str);

        void processConsumerReceivedMessage(
            const std::string& str,
            const OnSubscribeResponseCallback& callback,
            std::shared_ptr<WebSocketClient> webSocketClient);

        std::string createContext();
        
        std::string serializePublishMessage(
            const std::string& str,
            const std::string& context);

        void createQueueProcessor();

        std::map<std::string, std::shared_ptr<WebSocketClient>> _clients;
        std::map<std::string, OnPublishResponseCallback> _publishCallbacks;
        std::map<std::string, std::shared_ptr<uvw::TimerHandle>> _publishTimers;
        std::string _baseUrl;
        uint64_t _mId;

        // We could have multiple queues per topic
        // We could use a std::deque too.
        std::list<std::pair<std::string, std::string>> _queue;
        size_t _maxQueueSize;
        static const size_t _defaultMaxQueueSize;

        std::shared_ptr<uvw::TimerHandle> _timer;

        // Stats
        size_t _droppedMessages;
        size_t _deliveredMessages;
    };
}
