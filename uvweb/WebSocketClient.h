
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
        int port;
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
        ~WebSocketClient();

        void setOnMessageCallback(const OnMessageCallback& callback);
        void invokeOnMessageCallback(const WebSocketMessagePtr& msg);

        void connect(const std::string& url);

        bool send(const std::string& data, bool binary);
        bool sendBinary(const std::string& text);
        bool sendText(const std::string& text);
        bool ping(const std::string& text);

        void close(uint16_t code = WebSocketCloseConstants::kNormalClosureCode,
                   const std::string& reason = WebSocketCloseConstants::kNormalClosureMessage,
                   size_t closeWireSize = 0,
                   bool remote = false);

        static std::string readyStateToString(ReadyState readyState);
        bool isConnected() const;

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

        enum class MessageKind
        {
            MSG_TEXT,
            MSG_BINARY,
            PING,
            PONG,
            FRAGMENT
        };

        void connect(const sockaddr& addr);

        bool writeHandshakeRequest();

        //
        // Sending data
        //
        bool sendData(wsheader_type::opcode_type type,
                      const std::string& message,
                      bool compress = false);

        bool prepareFragment(wsheader_type::opcode_type type,
                             bool fin,
                             std::string::const_iterator message_begin,
                             std::string::const_iterator message_end,
                             bool compress = false); // compress not implemented now

        bool sendFragment(const std::vector<uint8_t>& header,
                          std::string::const_iterator begin,
                          std::string::const_iterator end,
                          uint64_t message_size,
                          uint8_t masking_key[4]);

        bool sendOnSocket(const std::string& str);
        bool sendOnSocket(const std::vector<uint8_t>& vec);

        unsigned getRandomUnsigned();

        void setReadyState(ReadyState readyState);

        //
        // Receiving data
        //
        void dispatch(const uvw::DataEvent& event);

        void emitMessage(MessageKind messageKind,
                         const std::string& message,
                         bool compressedMessage);

        std::string getMergedChunks() const;

        void unmaskReceiveBuffer(const wsheader_type& ws);

        void handleReadError();

        //
        // Closing connection
        //
        void closeSocket();
        void closeSocketAndSwitchToClosedState(uint16_t code,
                                               const std::string& reason,
                                               size_t closeWireSize,
                                               bool remote);

        void setCloseReason(const std::string& reason);
        const std::string& getCloseReason() const;
        void sendCloseFrame(uint16_t code, const std::string& reason);

        //
        // Automatic reconnection
        //
        void startReconnectTimer();
        void stopReconnectTimer();

        //
        // Member variables
        //
        std::shared_ptr<uvw::TCPHandle> mClient;
        std::shared_ptr<http_parser> mHttpParser;
        http_parser_settings mSettings;

        Request mRequest;

        // 'the' callback
        OnMessageCallback _onMessageCallback;

        // Fragments are 32K long
        static constexpr size_t kChunkSize = 1 << 15;

        // Hold the state of the connection (OPEN, CLOSED, etc...)
        ReadyState _readyState;

        // In handshake
        bool mHandshaked;

        std::string _closeReason;
        uint16_t _closeCode;
        size_t _closeWireSize;
        bool _closeRemote;

        // Data used for Per Message Deflate compression (with zlib)
        bool _enablePerMessageDeflate;

        // Tells whether we should mask the data we send.
        // client should mask but server should not
        bool _useMask;

        // Contains all messages that were fetched in the last socket read.
        // This could be a mix of control messages (Close, Ping, etc...) and
        // data messages. That buffer
        std::vector<uint8_t> _rxbuf;

        // Hold fragments for multi-fragments messages in a list. We support receiving very large
        // messages (tested messages up to 700M) and we cannot put them in a single
        // buffer that is resized, as this operation can be slow when a buffer has its
        // size increased 2 fold, while appending to a list has a fixed cost.
        std::list<std::string> _chunks;

        // Record the message kind (will be TEXT or BINARY) for a fragmented
        // message, present in the first chunk, since the final chunk will be a
        // CONTINUATION opcode and doesn't tell the full message kind
        MessageKind _fragmentedMessageKind;

        // Ditto for whether a message is compressed
        bool _receivedMessageCompressed;

        std::chrono::time_point<std::chrono::steady_clock> _closingTimePoint;
        static const int kClosingMaximumWaitingDelayInMs;

        // enable auto response to ping
        bool _enablePong;
        static const bool kDefaultEnablePong;

        // Optional ping and pong timeout
        int _pingIntervalSecs;
        bool _pongReceived;

        static const int kDefaultPingIntervalSecs;
        static const std::string kPingMessage;
        uint64_t _pingCount;

        // automatic reconnection
        std::string _url;
        std::shared_ptr<uvw::TimerHandle> _automaticReconnectionTimer;
    };
}
