
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

    void PulsarClient::send(const std::string& str,
                            const std::string& tenant,
                            const std::string& nameSpace,
                            const std::string& topic,
                            const OnResponseCallback& callback)
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
        _callbacks[context] = callback;

        auto [hasClient, webSocketClient] = getWebSocketClient(url);
        if (!hasClient)
        {
            webSocketClient->setOnMessageCallback([this](const uvweb::WebSocketMessagePtr& msg) {
                if (msg->type == uvweb::WebSocketMessageType::Message)
                {
                    processReceivedMessage(msg->str);
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

    std::string PulsarClient::createContext()
    {
        return std::to_string(_mId++);
    }

    std::pair<bool, std::shared_ptr<WebSocketClient>> PulsarClient::getWebSocketClient(const std::string& key)
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

    std::string PulsarClient::serializePublishMessage(
        const std::string& str,
        const std::string& context)
    {
        std::string payload = base64_encode(str, str.size());
        nlohmann::json data = {
            {"payload", payload}, {"context", context}, {"properties", {{"key1", "val1"}}}};

        return data.dump();
    }

    void PulsarClient::processReceivedMessage(const std::string& str)
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
        auto it = _callbacks.find(context);
        if (it != _callbacks.end())
        {
            auto callback = it->second;
            callback(true, pdu.value("messageId", "n/a"));
            _callbacks.erase(context);
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

        _timer->on<uvw::TimerEvent>([this](const auto &, auto &hndl){
            // auto data = std::make_unique<char[]>('*');
            // handle->write(std::move(data), 1);
            // hndl.close();

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
        _timer->start(uvw::TimerHandle::Time{0}, uvw::TimerHandle::Time{100});
    }
} // namespace uvweb
