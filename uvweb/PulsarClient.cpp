
//
// Talks to a Pulsar WebSocket Broker
// See https://pulsar.apache.org/docs/en/client-libraries-websocket/
//

#include "PulsarClient.h"

#include "Base64.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

namespace uvweb
{
    PulsarClient::PulsarClient(const std::string& baseUrl)
        : _baseUrl(baseUrl)
        , _mId(0)
    {
        createQueueProcessor();
    }

    void PulsarClient::publish(const std::string& str,
                               const std::string& tenant,
                               const std::string& nameSpace,
                               const std::string& topic,
                               const OnPublishResponseCallback& callback)
    {
        //
        // ws://broker-service-url:8080/ws/v2/producer/persistent/:tenant/:namespace/:topic
        //
        std::stringstream ss;
        ss << _baseUrl << "/ws/v2/producer/persistent/" << tenant << "/" << nameSpace << "/"
           << topic;
        std::string url(ss.str());

        // Keep track of the callback
        auto context = createContext();
        _publishCallbacks[context] = callback;

        auto [hasClient, webSocketClient] = getWebSocketClient(url);
        if (!hasClient)
        {
            webSocketClient->setOnMessageCallback([this](const uvweb::WebSocketMessagePtr& msg) {
                if (msg->type == uvweb::WebSocketMessageType::Message)
                {
                    processProducerReceivedMessage(msg->str);
                }
                else if (msg->type == uvweb::WebSocketMessageType::Open)
                {
                    spdlog::debug("Connection to {} established", _baseUrl);
                }
                else if (msg->type == uvweb::WebSocketMessageType::Close)
                {
                    spdlog::debug("Connection to {} closed", _baseUrl);
                }
            });

            webSocketClient->connect(url);
        }

        auto serializedMsg = serializePublishMessage(str, context);
        _queue.push({url, serializedMsg});
    }

    void PulsarClient::subscribe(const std::string& tenant,
                                 const std::string& nameSpace,
                                 const std::string& topic,
                                 const std::string& subscription,
                                 const OnSubscribeResponseCallback& callback)
    {
        //
        // TOPIC = 'ws://localhost:8080/ws/v2/consumer/persistent/public/default/my-topic/my-sub'
        //
        std::stringstream ss;
        ss << _baseUrl << "/ws/v2/consumer/persistent/" << tenant << "/" << nameSpace << "/"
           << topic << "/" << subscription;
        std::string url(ss.str());

        auto [hasClient, webSocketClient] = getWebSocketClient(url);
        if (!hasClient) // FIXME: how do we handle multiple subscriptions ?
        {
            webSocketClient->setOnMessageCallback(
                [this, callback, url](const uvweb::WebSocketMessagePtr& msg) {
                    if (msg->type == uvweb::WebSocketMessageType::Message)
                    {
                        auto [hasClient, webSocketClient] = getWebSocketClient(url);
                        processConsumerReceivedMessage(msg->str, callback, webSocketClient);
                    }
                    else if (msg->type == uvweb::WebSocketMessageType::Open)
                    {
                        spdlog::debug("Connection to {} established", _baseUrl);
                    }
                    else if (msg->type == uvweb::WebSocketMessageType::Close)
                    {
                        spdlog::debug("Connection to {} closed", _baseUrl);
                    }
                });

            webSocketClient->connect(url);
        }
    }

    std::string PulsarClient::createContext()
    {
        return std::to_string(_mId++);
    }

    std::pair<bool, std::shared_ptr<WebSocketClient>> PulsarClient::getWebSocketClient(
        const std::string& key)
    {
        auto it = _clients.find(key);
        if (it != _clients.end())
        {
            return {true, it->second};
        }

        auto client = std::make_shared<WebSocketClient>();
        _clients[key] = client;
        return {false, client};
    }

    std::string PulsarClient::serializePublishMessage(const std::string& str,
                                                      const std::string& context)
    {
        std::string payload = base64_encode(str, str.size());
        nlohmann::json data = {
            {"payload", payload}, {"context", context}, {"properties", {{"key1", "val1"}}}};

        return data.dump();
    }

    void PulsarClient::processProducerReceivedMessage(const std::string& str)
    {
        spdlog::debug("received message: {}", str);

        nlohmann::json pdu;
        try
        {
            pdu = nlohmann::json::parse(str);
            spdlog::debug("message is valid json");
        }
        catch (const nlohmann::json::parse_error& e)
        {
            spdlog::error("malformed json pdu: {}, error: {}", str, e.what());
            return;
        }

        auto success = pdu.value("result", "n/a");
        if (success != "ok")
        {
            spdlog::error("error response: {}", success);
            return;
        }

        auto context = pdu.value("context", "n/a");
        auto it = _publishCallbacks.find(context);
        if (it != _publishCallbacks.end())
        {
            auto callback = it->second;
            callback(true, pdu.value("messageId", "n/a"));
            _publishCallbacks.erase(context);
        }
        else
        {
            spdlog::warn("orphan context: {}", context);
        }
    }

    void PulsarClient::createQueueProcessor()
    {
        auto loop = uvw::Loop::getDefault();
        _timer = loop->resource<uvw::TimerHandle>();

        _timer->on<uvw::TimerEvent>([this](const auto&, auto& hndl) {
            while (!_queue.empty())
            {
                auto item = _queue.front();
                auto [url, serializedMsg] = item;
                auto [hasClient, webSocketClient] = getWebSocketClient(url);
                if (!webSocketClient->isConnected()) return;

                if (webSocketClient->sendText(serializedMsg))
                {
                    _queue.pop();
                }
                else
                {
                    spdlog::debug("Error sending data to {}", url); // will retry
                }
            }
        });
        _timer->start(uvw::TimerHandle::Time {0}, uvw::TimerHandle::Time {100});
    }

    void PulsarClient::processConsumerReceivedMessage(
        const std::string& str,
        const OnSubscribeResponseCallback& callback,
        std::shared_ptr<WebSocketClient> webSocketClient)
    {
        spdlog::debug("received message: {}", str);

        nlohmann::json pdu;
        try
        {
            pdu = nlohmann::json::parse(str);
            spdlog::debug("message is valid json");
        }
        catch (const nlohmann::json::parse_error& e)
        {
            spdlog::error("malformed json pdu: {}, error: {}", str, e.what());
            return;
        }

        auto payload = pdu.value("payload", "n/a"); // FIXME a
        payload = base64_decode(payload);

        auto messageId = pdu.value("messageId", "n/a");

        if (callback(payload, messageId))
        {
            nlohmann::json data = {{"messageId", messageId}};

            // Acknowledge message
            if (!webSocketClient->sendText(data.dump()))
            {
                spdlog::error("Error acknowledging message id {}", messageId);
            }
        }
    }
} // namespace uvweb
