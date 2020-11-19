
#pragma once

#include "WebSocketMessage.h"
#include "WebSocketCloseConstants.h"

#include <string>
#include <map>
#include <vector>
#include <functional>

#include <uvw.hpp>
#include "http_parser.h"

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

    enum class SendMessageKind
    {
        Text,
        Binary,
        Ping
    };

    using OnMessageCallback = std::function<void(const WebSocketMessagePtr&)>;

    class WebSocketClient
    {
    public:
        WebSocketClient();
        void connect(const std::string& url,
                     const OnMessageCallback& callback);

        bool send(const std::string& data, bool binary);
        bool sendBinary(const std::string& text);
        bool sendText(const std::string& text);
        bool ping(const std::string& text);

        void close(uint16_t code = WebSocketCloseConstants::kNormalClosureCode,
                   const std::string& reason = WebSocketCloseConstants::kNormalClosureMessage);

    private:
        struct wsheader_type
        {
            unsigned header_size;
            bool fin;
            bool rsv1;
            bool rsv2;
            bool rsv3;
            bool mask;
            enum opcode_type
            {
                CONTINUATION = 0x0,
                TEXT_FRAME = 0x1,
                BINARY_FRAME = 0x2,
                CLOSE = 8,
                PING = 9,
                PONG = 0xa,
            } opcode;
            int N0;
            uint64_t N;
            uint8_t masking_key[4];
        };

        void writeHandshakeRequest(uvw::TCPHandle& client);

        bool sendData(wsheader_type::opcode_type type,
                      const std::string& message);

        bool sendFragment(wsheader_type::opcode_type type,
                          bool fin,
                          std::string::const_iterator message_begin,
                          std::string::const_iterator message_end,
                          bool compress = false); // compress not implemented now

        void appendToSendBuffer(const std::vector<uint8_t>& header,
                                std::string::const_iterator begin,
                                std::string::const_iterator end,
                                uint64_t message_size,
                                uint8_t masking_key[4]);

        bool sendOnSocket();

        unsigned getRandomUnsigned();

        void setReadyState(ReadyState readyState);

        //
        // Member variables
        //
        std::shared_ptr<uvw::TCPHandle> mClient;
        http_parser_settings mSettings;
        http_parser* mHttpParser;

        Request mRequest;

        // Contains all messages that are waiting to be sent
        std::vector<uint8_t> _txbuf;

        // Fragments are 32K long
        static constexpr size_t kChunkSize = 1 << 15;

        // Hold the state of the connection (OPEN, CLOSED, etc...)
        ReadyState _readyState;

        std::string _closeReason;
        uint16_t _closeCode;
        size_t _closeWireSize;
        bool _closeRemote;

        // Tells whether we should mask the data we send.
        // client should mask but server should not
        bool _useMask;

    };
}
