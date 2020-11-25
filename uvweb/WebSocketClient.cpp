


#include "WebSocketClient.h"

#include "UrlParser.h"
#include "Utf8Validator.h"
#include "gzip.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>

namespace uvweb
{
    std::string userAgent()
    {
        return "uvweb";
    }

    int on_message_begin(http_parser*)
    {
        return 0;
    }

    int on_status(http_parser* parser, const char* at, const size_t length)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        response->statusCode = parser->status_code;
        return 0;
    }

    int on_headers_complete(http_parser* parser)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);

        for (const auto& it : response->headers)
        {
            spdlog::debug("{}: {}", it.first, it.second);
        }

        return 0;
    }

    int on_message_complete(http_parser* parser)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        response->messageComplete = true;

        if (response->headers["Content-Encoding"] == "gzip")
        {
            spdlog::debug("decoding gzipped body");

            std::string decompressedBody;
            if (!gzipDecompress(response->body, decompressedBody))
            {
                return 1;
            }
            response->body = decompressedBody;
        }

        spdlog::debug("body value {}", response->body);
        return 0;
    }

    int on_header_field(http_parser* parser, const char* at, const size_t length)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        response->currentHeaderName = std::string(at, length);

        spdlog::debug("on header field {}", response->currentHeaderName);
        return 0;
    }

    int on_header_value(http_parser* parser, const char* at, const size_t length)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        response->currentHeaderValue = std::string(at, length);

        response->headers[response->currentHeaderName] = response->currentHeaderValue;

        spdlog::debug("on header value {}", response->currentHeaderValue);
        return 0;
    }

    int on_body(http_parser* parser, const char* at, const size_t length)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        auto body = std::string(at, length);
        response->body += body;

        spdlog::debug("on body {}", body);
        return 0;
    }

    std::string genRandomString(const int len)
    {
        std::string alphanum = "0123456789"
                               "ABCDEFGH"
                               "abcdefgh";

        std::random_device r;
        std::default_random_engine e1(r());
        std::uniform_int_distribution<int> dist(0, (int) alphanum.size() - 1);

        std::string s;
        s.resize(len);

        for (int i = 0; i < len; ++i)
        {
            int x = dist(e1);
            s[i] = alphanum[x];
        }

        return s;
    }

    const std::string WebSocketClient::kPingMessage("ixwebsocket::heartbeat");
    const int WebSocketClient::kDefaultPingIntervalSecs(-1);
    const bool WebSocketClient::kDefaultEnablePong(true);
    const int WebSocketClient::kClosingMaximumWaitingDelayInMs(300);
    constexpr size_t WebSocketClient::kChunkSize;

    WebSocketClient::WebSocketClient()
        : _useMask(true)
        , _readyState(ReadyState::Closed)
        , _closeCode(WebSocketCloseConstants::kInternalErrorCode)
        , _enablePerMessageDeflate(false)
        , _receivedMessageCompressed(false)
        , mHandshaked(false)
        , _closingTimePoint(std::chrono::steady_clock::now())
        , _enablePong(kDefaultEnablePong)
        , _pingIntervalSecs(kDefaultPingIntervalSecs)
        , _pongReceived(false)
        , _pingCount(0)
    {
        // Register http parser callbacks
        memset(&mSettings, 0, sizeof(mSettings));
        mSettings.on_message_begin = on_message_begin;
        mSettings.on_status = on_status;
        mSettings.on_headers_complete = on_headers_complete;
        mSettings.on_message_complete = on_message_complete;
        mSettings.on_header_field = on_header_field;
        mSettings.on_header_value = on_header_value;
        mSettings.on_body = on_body;
    }

    WebSocketClient::~WebSocketClient()
    {
        ;
    }

    // FIXME: not hooked for when the remote connection is dropped
    void WebSocketClient::startReconnectTimer()
    {
        stopReconnectTimer();

        // Timer for automatic reconnection
        auto loop = uvw::Loop::getDefault();
        _automaticReconnectionTimer = loop->resource<uvw::TimerHandle>();

        _automaticReconnectionTimer->on<uvw::TimerEvent>([this](const auto&, auto&) {
            if (!isConnected() && !_url.empty())
            {
                spdlog::info("Trying to reconnect");
                spdlog::info("Current Ready state: {}",
                             WebSocketClient::readyStateToString(_readyState));
                connect(_url);
            }
        });
        _automaticReconnectionTimer->start(uvw::TimerHandle::Time {1000},
                                           uvw::TimerHandle::Time {0});
    }

    void WebSocketClient::stopReconnectTimer()
    {
        if (_automaticReconnectionTimer)
        {
            _automaticReconnectionTimer->stop();
        }
    }

    void WebSocketClient::connect(const std::string& url)
    {
        _url = url;
        std::string protocol, host, path, query;
        int port;

        if (!UrlParser::parse(url, protocol, host, path, query, port))
        {
            std::stringstream ss;
            ss << "Could not parse url: '" << url << "'";
            spdlog::error(ss.str());
            return;
        }

        stopReconnectTimer();
        auto loop = uvw::Loop::getDefault();
        mHandshaked = false;
        mClient = loop->resource<uvw::TCPHandle>();
        mHttpParser = std::make_shared<http_parser>();
        http_parser_init(mHttpParser.get(), HTTP_RESPONSE);

        auto dnsRequest = loop->resource<uvw::GetAddrInfoReq>();
        auto [dnsLookupSuccess, addr] =
            dnsRequest->addrInfoSync(host, std::to_string(port)); // FIXME: do DNS asynchronously
        if (!dnsLookupSuccess)
        {
            std::stringstream ss;
            ss << "Could not resolve host: '" << host << "'";
            spdlog::error(ss.str());
            return;
        }

        auto response = std::make_shared<Response>();
        mClient->data(response);

        mHttpParser->data = mClient->data().get();

        mRequest.path = path;
        mRequest.host = host;
        mRequest.port = port;

        // On Error
        mClient->on<uvw::ErrorEvent>(
            [this, host, port](const uvw::ErrorEvent& errorEvent, uvw::TCPHandle&) {
                spdlog::error("Connection to {}:{} failed : {}", host, port, errorEvent.name());

                // FIXME: maybe call handleReadError(), ported from ix ?
                startReconnectTimer();
            });

        // On connect
        mClient->once<uvw::ConnectEvent>([this](const uvw::ConnectEvent&, uvw::TCPHandle&) {
            if (!writeHandshakeRequest())
            {
                spdlog::error("Error sending handshake");
            }
        });

        mClient->once<uvw::WriteEvent>([](const uvw::WriteEvent&, uvw::TCPHandle& client) {
            spdlog::debug("Data written to socket");
        });

        mClient->on<uvw::DataEvent>([this, response](const uvw::DataEvent& event,
                                                     uvw::TCPHandle& client) {
            if (mHandshaked)
            {
                spdlog::debug("Received {} bytes", event.length);
                std::string msg(event.data.get(), event.length);
                dispatch(msg);
            }
            else
            {
                int nparsed = http_parser_execute(
                    mHttpParser.get(), &mSettings, event.data.get(), event.length);
                // Write response
                if (mHttpParser->upgrade)
                {
                    spdlog::info("HTTP Upgrade, status code: {}", response->statusCode);

                    // FIXME: missing validation of WebSocket Key header

                    mHandshaked = true;
                    stopReconnectTimer();
                    setReadyState(ReadyState::Open);

                    // emit connected callback
                    invokeOnMessageCallback(std::make_unique<WebSocketMessage>(
                        WebSocketMessageType::Open,
                        "",
                        0,
                        WebSocketErrorInfo(),
                        WebSocketOpenInfo(mRequest.path, response->headers, response->protocol),
                        WebSocketCloseInfo()));

                    // The input buffer might already contains some non HTTP data
                    if (nparsed < event.length)
                    {
                        auto offset = event.length - nparsed;
                        std::string msg(event.data.get() + nparsed, offset);
                        dispatch(msg);
                    }
                }
                else if (nparsed != event.length)
                {
                    std::stringstream ss;
                    ss << "HTTP Parsing Error: "
                       << "description: " << http_errno_description(HTTP_PARSER_ERRNO(mHttpParser))
                       << " error name " << http_errno_name(HTTP_PARSER_ERRNO(mHttpParser))
                       << " nparsed " << nparsed << " event.length " << event.length;
                    spdlog::error(ss.str());

                    std::string msg(event.data.get(), event.length);
                    spdlog::debug("Msg received: {}", msg);
                    return;
                }
                else if (response->messageComplete)
                {
                    spdlog::debug("response completed");
                }
            }
        });

        mClient->connect(*addr->ai_addr);
        mClient->read(); // necessary or nothing happens
    }

    bool WebSocketClient::writeHandshakeRequest()
    {
        // FIXME: this would be a good place to start this timer but it does not work
        // startReconnectTimer();

        // Write the request to the socket
        std::stringstream ss;
        //
        // Generate a random 24 bytes string which looks like it is base64 encoded
        // y3JJHMbDL1EzLkh9GBhXDw==
        // 0cb3Vd9HkbpVVumoS3Noka==
        //
        // See https://stackoverflow.com/questions/18265128/what-is-sec-websocket-key-for
        //
        std::string secWebSocketKey = genRandomString(22);
        secWebSocketKey += "==";

        // FIXME: only write those default headers if no user supplied are presents
        ss << "GET " << mRequest.path << " HTTP/1.1\r\n";
        ss << "Host: " << mRequest.host << ":" << mRequest.port << "\r\n";
        ss << "Upgrade: websocket\r\n";
        ss << "Connection: Upgrade\r\n";
        ss << "Sec-WebSocket-Version: 13\r\n";
        ss << "Sec-WebSocket-Key: " << secWebSocketKey << "\r\n";

        // User-Agent can be customized by users
        if (mRequest.headers.find("User-Agent") == mRequest.headers.end())
        {
            ss << "User-Agent: " << userAgent() << "\r\n";
        }

        for (auto&& it : mRequest.headers)
        {
            ss << it.first << ": " << it.second << "\r\n";
        }
        ss << "\r\n";

        spdlog::debug(ss.str());

        return sendOnSocket(ss.str());
    }

    bool WebSocketClient::sendOnSocket(const std::string& str)
    {
        spdlog::debug("sendOnSocket {} bytes", str.size());
        auto buff = std::make_unique<char[]>(str.length());
        std::copy_n(str.c_str(), str.length(), buff.get());

        mClient->write(std::move(buff), str.length());
        return true;
    }

    bool WebSocketClient::sendOnSocket(const std::vector<uint8_t>& vec)
    {
        spdlog::debug("sendOnSocket {} bytes", vec.size());
        auto buff = std::make_unique<char[]>(vec.size());
        std::copy_n(&vec.front(), vec.size(), buff.get());

        mClient->write(std::move(buff), vec.size());
        return true;
    }

    bool WebSocketClient::send(const std::string& data, bool binary)
    {
        return (binary) ? sendBinary(data) : sendText(data);
    }

    bool WebSocketClient::sendBinary(const std::string& text)
    {
        return sendData(wsheader_type::BINARY_FRAME, text);
    }

    bool WebSocketClient::sendText(const std::string& text)
    {
        if (!validateUtf8(text))
        {
            close(WebSocketCloseConstants::kInvalidFramePayloadData,
                  WebSocketCloseConstants::kInvalidFramePayloadDataMessage);
            return false;
        }
        return sendData(wsheader_type::TEXT_FRAME, text);
    }

    void WebSocketClient::close(uint16_t code,
                                const std::string& reason,
                                size_t closeWireSize,
                                bool remote)
    {
        if (_readyState == ReadyState::Closing || _readyState == ReadyState::Closed)
        {
            return;
        }

        if (!remote)
        {
            // FIXME: validate this
            stopReconnectTimer();
        }

        if (closeWireSize == 0)
        {
            closeWireSize = reason.size();
        }

        setCloseReason(reason);
        _closeCode = code;
        _closeWireSize = closeWireSize;
        _closeRemote = remote;

        _closingTimePoint = std::chrono::steady_clock::now();
        setReadyState(ReadyState::Closing);

        sendCloseFrame(code, reason);
    }

    void WebSocketClient::closeSocketAndSwitchToClosedState(uint16_t code,
                                                            const std::string& reason,
                                                            size_t closeWireSize,
                                                            bool remote)
    {
        closeSocket();

        setCloseReason(reason);
        _closeCode = code;
        _closeWireSize = closeWireSize;
        _closeRemote = remote;

        setReadyState(ReadyState::Closed);
    }

    void WebSocketClient::closeSocket()
    {
        mClient->close();
    }

    void WebSocketClient::sendCloseFrame(uint16_t code, const std::string& reason)
    {
        bool compress = false;

        // if a status is set/was read
        if (code != WebSocketCloseConstants::kNoStatusCodeErrorCode)
        {
            // See list of close events here:
            // https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent
            std::string closure {(char) (code >> 8), (char) (code & 0xff)};

            // copy reason after code
            closure.append(reason);

            sendData(wsheader_type::CLOSE, closure, compress);
        }
        else
        {
            // no close code/reason set
            sendData(wsheader_type::CLOSE, std::string(""), compress);
        }
    }

    bool WebSocketClient::sendData(wsheader_type::opcode_type type,
                                   const std::string& message,
                                   bool compress)
    {
        if (_readyState != ReadyState::Open && _readyState != ReadyState::Closing)
        {
            return false;
        }

        size_t wireSize = message.size();
        auto message_begin = message.cbegin();
        auto message_end = message.cend();

        bool success = true;

        // Common case for most message. No fragmentation required.
        if (wireSize < kChunkSize)
        {
            success = prepareFragment(type, true, message_begin, message_end, compress);
        }
        else
        {
            //
            // Large messages need to be fragmented
            //
            // Rules:
            // First message needs to specify a proper type (BINARY or TEXT)
            // Intermediary and last messages need to be of type CONTINUATION
            // Last message must set the fin byte.
            //
            auto steps = wireSize / kChunkSize;

            std::string::const_iterator begin = message_begin;
            std::string::const_iterator end = message_end;

            for (uint64_t i = 0; i < steps; ++i)
            {
                bool firstStep = i == 0;
                bool lastStep = (i + 1) == steps;
                bool fin = lastStep;

                end = begin + kChunkSize;
                if (lastStep)
                {
                    end = message_end;
                }

                auto opcodeType = type;
                if (!firstStep)
                {
                    opcodeType = wsheader_type::CONTINUATION;
                }

                // Send message
                if (!prepareFragment(opcodeType, fin, begin, end, compress))
                {
                    return false;
                }

                begin += kChunkSize;
            }
        }

        return true;
    }

    bool WebSocketClient::prepareFragment(wsheader_type::opcode_type type,
                                          bool fin,
                                          std::string::const_iterator message_begin,
                                          std::string::const_iterator message_end,
                                          bool compress)
    {
        uint64_t message_size = static_cast<uint64_t>(message_end - message_begin);

        unsigned x = getRandomUnsigned();
        uint8_t masking_key[4] = {};
        masking_key[0] = (x >> 24);
        masking_key[1] = (x >> 16) & 0xff;
        masking_key[2] = (x >> 8) & 0xff;
        masking_key[3] = (x) &0xff;

        std::vector<uint8_t> header;
        header.assign(2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) +
                          (_useMask ? 4 : 0),
                      0);
        header[0] = type;

        // The fin bit indicate that this is the last fragment. Fin is French for end.
        if (fin)
        {
            header[0] |= 0x80;
        }

        // The rsv1 bit indicate that the frame is compressed
        // continuation opcodes should not set it. Autobahn 12.2.10 and others 12.X
        if (compress && type != wsheader_type::CONTINUATION)
        {
            header[0] |= 0x40;
        }

        if (message_size < 126)
        {
            header[1] = (message_size & 0xff) | (_useMask ? 0x80 : 0);

            if (_useMask)
            {
                header[2] = masking_key[0];
                header[3] = masking_key[1];
                header[4] = masking_key[2];
                header[5] = masking_key[3];
            }
        }
        else if (message_size < 65536)
        {
            header[1] = 126 | (_useMask ? 0x80 : 0);
            header[2] = (message_size >> 8) & 0xff;
            header[3] = (message_size >> 0) & 0xff;

            if (_useMask)
            {
                header[4] = masking_key[0];
                header[5] = masking_key[1];
                header[6] = masking_key[2];
                header[7] = masking_key[3];
            }
        }
        else
        { // TODO: run coverage testing here
            header[1] = 127 | (_useMask ? 0x80 : 0);
            header[2] = (message_size >> 56) & 0xff;
            header[3] = (message_size >> 48) & 0xff;
            header[4] = (message_size >> 40) & 0xff;
            header[5] = (message_size >> 32) & 0xff;
            header[6] = (message_size >> 24) & 0xff;
            header[7] = (message_size >> 16) & 0xff;
            header[8] = (message_size >> 8) & 0xff;
            header[9] = (message_size >> 0) & 0xff;

            if (_useMask)
            {
                header[10] = masking_key[0];
                header[11] = masking_key[1];
                header[12] = masking_key[2];
                header[13] = masking_key[3];
            }
        }

        return sendFragment(header, message_begin, message_end, message_size, masking_key);
    }

    bool WebSocketClient::sendFragment(const std::vector<uint8_t>& header,
                                       std::string::const_iterator begin,
                                       std::string::const_iterator end,
                                       uint64_t message_size,
                                       uint8_t masking_key[4])
    {
        // Contains all messages that are waiting to be sent
        std::vector<uint8_t> txbuf;

        txbuf.insert(txbuf.end(), header.begin(), header.end());
        txbuf.insert(txbuf.end(), begin, end);

        if (_useMask)
        {
            for (size_t i = 0; i != (size_t) message_size; ++i)
            {
                *(txbuf.end() - (size_t) message_size + i) ^= masking_key[i & 0x3];
            }
        }

        // Now actually send this data
        return sendOnSocket(txbuf);
    }

    void WebSocketClient::unmaskReceiveBuffer(const wsheader_type& ws)
    {
        if (ws.mask)
        {
            for (size_t j = 0; j != ws.N; ++j)
            {
                _rxbuf[j + ws.header_size] ^= ws.masking_key[j & 0x3];
            }
        }
    }

    unsigned WebSocketClient::getRandomUnsigned()
    {
        auto now = std::chrono::system_clock::now();
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        return static_cast<unsigned>(seconds);
    }

    void WebSocketClient::setReadyState(ReadyState readyState)
    {
        // No state change, return
        if (_readyState == readyState) return;

        auto wireSize = 0;

        if (readyState == ReadyState::Closed)
        {
            invokeOnMessageCallback(std::make_unique<WebSocketMessage>(
                WebSocketMessageType::Close,
                "",
                wireSize,
                WebSocketErrorInfo(),
                WebSocketOpenInfo(),
                WebSocketCloseInfo(_closeCode, getCloseReason(), _closeRemote)));

            setCloseReason(WebSocketCloseConstants::kInternalErrorMessage);
            _closeCode = WebSocketCloseConstants::kInternalErrorCode;
            _closeWireSize = 0;
            _closeRemote = false;
        }
        else if (readyState == ReadyState::Open)
        {
#if 0
            // initTimePointsAfterConnect();
#endif
            _pongReceived = false;
        }

        _readyState = readyState;

        spdlog::debug("New Ready state: {}", WebSocketClient::readyStateToString(_readyState));
    }

    //
    // http://tools.ietf.org/html/rfc6455#section-5.2  Base Framing Protocol
    //
    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-------+-+-------------+-------------------------------+
    // |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
    // |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
    // |N|V|V|V|       |S|             |   (if payload len==126/127)   |
    // | |1|2|3|       |K|             |                               |
    // +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    // |     Extended payload length continued, if payload len == 127  |
    // + - - - - - - - - - - - - - - - +-------------------------------+
    // |                               |Masking-key, if MASK set to 1  |
    // +-------------------------------+-------------------------------+
    // | Masking-key (continued)       |          Payload Data         |
    // +-------------------------------- - - - - - - - - - - - - - - - +
    // :                     Payload Data continued ...                :
    // + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
    // |                     Payload Data continued ...                |
    // +---------------------------------------------------------------+
    //
    void WebSocketClient::dispatch(const std::string& buffer)
    {
        //
        // Append the incoming data to our _rxbuf receive buffer.
        //
        _rxbuf.insert(_rxbuf.end(), buffer.begin(), buffer.end());

        while (true)
        {
            wsheader_type ws;
            if (_rxbuf.size() < 2) break;                /* Need at least 2 */
            const uint8_t* data = (uint8_t*) &_rxbuf[0]; // peek, but don't consume
            ws.fin = (data[0] & 0x80) == 0x80;
            ws.rsv1 = (data[0] & 0x40) == 0x40;
            ws.rsv2 = (data[0] & 0x20) == 0x20;
            ws.rsv3 = (data[0] & 0x10) == 0x10;
            ws.opcode = (wsheader_type::opcode_type)(data[0] & 0x0f);
            ws.mask = (data[1] & 0x80) == 0x80;
            ws.N0 = (data[1] & 0x7f);
            ws.header_size =
                2 + (ws.N0 == 126 ? 2 : 0) + (ws.N0 == 127 ? 8 : 0) + (ws.mask ? 4 : 0);
            if (_rxbuf.size() < ws.header_size) break; /* Need: ws.header_size - _rxbuf.size() */

            if ((ws.rsv1 && !_enablePerMessageDeflate) || ws.rsv2 || ws.rsv3)
            {
                close(WebSocketCloseConstants::kProtocolErrorCode,
                      WebSocketCloseConstants::kProtocolErrorReservedBitUsed,
                      _rxbuf.size());
                return;
            }

            //
            // Calculate payload length:
            // 0-125 mean the payload is that long.
            // 126 means that the following two bytes indicate the length,
            // 127 means the next 8 bytes indicate the length.
            //
            int i = 0;
            if (ws.N0 < 126)
            {
                ws.N = ws.N0;
                i = 2;
            }
            else if (ws.N0 == 126)
            {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 8;
                ws.N |= ((uint64_t) data[3]) << 0;
                i = 4;
            }
            else if (ws.N0 == 127)
            {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 56;
                ws.N |= ((uint64_t) data[3]) << 48;
                ws.N |= ((uint64_t) data[4]) << 40;
                ws.N |= ((uint64_t) data[5]) << 32;
                ws.N |= ((uint64_t) data[6]) << 24;
                ws.N |= ((uint64_t) data[7]) << 16;
                ws.N |= ((uint64_t) data[8]) << 8;
                ws.N |= ((uint64_t) data[9]) << 0;
                i = 10;
            }
            else
            {
                // invalid payload length according to the spec. bail out
                return;
            }

            if (ws.mask)
            {
                ws.masking_key[0] = ((uint8_t) data[i + 0]) << 0;
                ws.masking_key[1] = ((uint8_t) data[i + 1]) << 0;
                ws.masking_key[2] = ((uint8_t) data[i + 2]) << 0;
                ws.masking_key[3] = ((uint8_t) data[i + 3]) << 0;
            }
            else
            {
                ws.masking_key[0] = 0;
                ws.masking_key[1] = 0;
                ws.masking_key[2] = 0;
                ws.masking_key[3] = 0;
            }

            // Prevent integer overflow in the next conditional
            const uint64_t maxFrameSize(1ULL << 63);
            if (ws.N > maxFrameSize)
            {
                return;
            }

            if (_rxbuf.size() < ws.header_size + ws.N)
            {
                return; /* Need: ws.header_size+ws.N - _rxbuf.size() */
            }

            if (!ws.fin && (ws.opcode == wsheader_type::PING || ws.opcode == wsheader_type::PONG ||
                            ws.opcode == wsheader_type::CLOSE))
            {
                // Control messages should not be fragmented
                close(WebSocketCloseConstants::kProtocolErrorCode,
                      WebSocketCloseConstants::kProtocolErrorCodeControlMessageFragmented);
                return;
            }

            unmaskReceiveBuffer(ws);
            std::string frameData(_rxbuf.begin() + ws.header_size,
                                  _rxbuf.begin() + ws.header_size + (size_t) ws.N);

            // We got a whole message, now do something with it:
            if (ws.opcode == wsheader_type::TEXT_FRAME ||
                ws.opcode == wsheader_type::BINARY_FRAME ||
                ws.opcode == wsheader_type::CONTINUATION)
            {
                if (ws.opcode != wsheader_type::CONTINUATION)
                {
                    _fragmentedMessageKind = (ws.opcode == wsheader_type::TEXT_FRAME)
                                                 ? MessageKind::MSG_TEXT
                                                 : MessageKind::MSG_BINARY;

                    _receivedMessageCompressed = _enablePerMessageDeflate && ws.rsv1;

                    // Continuation message needs to follow a non-fin TEXT or BINARY message
                    if (!_chunks.empty())
                    {
                        close(WebSocketCloseConstants::kProtocolErrorCode,
                              WebSocketCloseConstants::kProtocolErrorCodeDataOpcodeOutOfSequence);
                    }
                }
                else if (_chunks.empty())
                {
                    // Continuation message need to follow a non-fin TEXT or BINARY message
                    close(
                        WebSocketCloseConstants::kProtocolErrorCode,
                        WebSocketCloseConstants::kProtocolErrorCodeContinuationOpCodeOutOfSequence);
                }

                //
                // Usual case. Small unfragmented messages
                //
                if (ws.fin && _chunks.empty())
                {
                    emitMessage(_fragmentedMessageKind, frameData, _receivedMessageCompressed);

                    _receivedMessageCompressed = false;
                }
                else
                {
                    //
                    // Add intermediary message to our chunk list.
                    // We use a chunk list instead of a big buffer because resizing
                    // large buffer can be very costly when we need to re-allocate
                    // the internal buffer which is slow and can let the internal OS
                    // receive buffer fill out.
                    //
                    _chunks.emplace_back(frameData);

                    if (ws.fin)
                    {
                        emitMessage(
                            _fragmentedMessageKind, getMergedChunks(), _receivedMessageCompressed);

                        _chunks.clear();
                        _receivedMessageCompressed = false;
                    }
                    else
                    {
                        emitMessage(MessageKind::FRAGMENT, std::string(), false);
                    }
                }
            }
            else if (ws.opcode == wsheader_type::PING)
            {
                // too large
                if (frameData.size() > 125)
                {
                    // Unexpected frame type
                    close(WebSocketCloseConstants::kProtocolErrorCode,
                          WebSocketCloseConstants::kProtocolErrorPingPayloadOversized);
                    return;
                }

                if (_enablePong)
                {
                    // Reply back right away
                    bool compress = false;
                    sendData(wsheader_type::PONG, frameData, compress);
                }

                emitMessage(MessageKind::PING, frameData, false);
            }
            else if (ws.opcode == wsheader_type::PONG)
            {
                _pongReceived = true;
                emitMessage(MessageKind::PONG, frameData, false);
            }
            else if (ws.opcode == wsheader_type::CLOSE)
            {
                std::string reason;
                uint16_t code = 0;

                if (ws.N >= 2)
                {
                    // Extract the close code first, available as the first 2 bytes
                    code |= ((uint64_t) _rxbuf[ws.header_size]) << 8;
                    code |= ((uint64_t) _rxbuf[ws.header_size + 1]) << 0;

                    // Get the reason.
                    if (ws.N > 2)
                    {
                        reason = frameData.substr(2, frameData.size());
                    }

                    // Validate that the reason is proper utf-8. Autobahn 7.5.1
                    if (!validateUtf8(reason))
                    {
                        code = WebSocketCloseConstants::kInvalidFramePayloadData;
                        reason = WebSocketCloseConstants::kInvalidFramePayloadDataMessage;
                    }

                    //
                    // Validate close codes. Autobahn 7.9.*
                    // 1014, 1015 are debattable. The firefox MSDN has a description for them.
                    // Full list of status code and status range is defined in the dedicated
                    // RFC section at https://tools.ietf.org/html/rfc6455#page-45
                    //
                    if (code < 1000 || code == 1004 || code == 1006 || (code > 1013 && code < 3000))
                    {
                        // build up an error message containing the bad error code
                        std::stringstream ss;
                        ss << WebSocketCloseConstants::kInvalidCloseCodeMessage << ": " << code;
                        reason = ss.str();

                        code = WebSocketCloseConstants::kProtocolErrorCode;
                    }
                }
                else
                {
                    // no close code received
                    code = WebSocketCloseConstants::kNoStatusCodeErrorCode;
                    reason = WebSocketCloseConstants::kNoStatusCodeErrorMessage;
                }

                // We receive a CLOSE frame from remote and are NOT the ones who triggered the close
                if (_readyState != ReadyState::Closing)
                {
                    // send back the CLOSE frame
                    sendCloseFrame(code, reason);

                    // FIXME delete ?
                    // wakeUpFromPoll(SelectInterrupt::kCloseRequest);

                    bool remote = true;
                    closeSocketAndSwitchToClosedState(code, reason, _rxbuf.size(), remote);
                }
                else
                {
                    // we got the CLOSE frame answer from our close, so we can close the connection
                    // if the code/reason are the same
                    bool identicalReason = _closeCode == code && getCloseReason() == reason;

                    if (identicalReason)
                    {
                        bool remote = false;
                        closeSocketAndSwitchToClosedState(code, reason, _rxbuf.size(), remote);
                    }
                }
            }
            else
            {
                // Unexpected frame type
                close(WebSocketCloseConstants::kProtocolErrorCode,
                      WebSocketCloseConstants::kProtocolErrorMessage,
                      _rxbuf.size());
            }

            // Erase the message that has been processed from the input/read buffer
            _rxbuf.erase(_rxbuf.begin(), _rxbuf.begin() + ws.header_size + (size_t) ws.N);
        }
    }

    void WebSocketClient::handleReadError()
    {
        // if an abnormal closure was raised in poll, and nothing else triggered a CLOSED state in
        // the received and processed data then close the connection
        _rxbuf.clear();

        // if we previously closed the connection (CLOSING state), then set state to CLOSED
        // (code/reason were set before)
        if (_readyState == ReadyState::Closing)
        {
            closeSocket();
            setReadyState(ReadyState::Closed);
        }
        // if we weren't closing, then close using abnormal close code and message
        else if (_readyState != ReadyState::Closed)
        {
            closeSocketAndSwitchToClosedState(WebSocketCloseConstants::kAbnormalCloseCode,
                                              WebSocketCloseConstants::kAbnormalCloseMessage,
                                              0,
                                              false);
        }
    }

    std::string WebSocketClient::getMergedChunks() const
    {
        size_t length = 0;
        for (auto&& chunk : _chunks)
        {
            length += chunk.size();
        }

        std::string msg;
        msg.reserve(length);

        for (auto&& chunk : _chunks)
        {
            msg += chunk;
        }

        return msg;
    }

    void WebSocketClient::emitMessage(MessageKind messageKind,
                                      const std::string& message,
                                      bool compressedMessage)
    {
        WebSocketMessageType webSocketMessageType;
        switch (messageKind)
        {
            case MessageKind::MSG_TEXT:
            case MessageKind::MSG_BINARY:
            {
                webSocketMessageType = WebSocketMessageType::Message;
            }
            break;

            case MessageKind::PING:
            {
                webSocketMessageType = WebSocketMessageType::Ping;
            }
            break;

            case MessageKind::PONG:
            {
                webSocketMessageType = WebSocketMessageType::Pong;
            }
            break;

            case MessageKind::FRAGMENT:
            {
                webSocketMessageType = WebSocketMessageType::Fragment;
            }
            break;
        }

        WebSocketErrorInfo webSocketErrorInfo;

        bool binary = messageKind == MessageKind::MSG_BINARY;
        size_t wireSize = message.size(); // FIXME zlib compression support

        invokeOnMessageCallback(std::make_unique<WebSocketMessage>(webSocketMessageType,
                                                                   message,
                                                                   wireSize,
                                                                   webSocketErrorInfo,
                                                                   WebSocketOpenInfo(),
                                                                   WebSocketCloseInfo(),
                                                                   binary));
    }

    void WebSocketClient::setCloseReason(const std::string& reason)
    {
        _closeReason = reason;
    }

    const std::string& WebSocketClient::getCloseReason() const
    {
        return _closeReason;
    }

    void WebSocketClient::setOnMessageCallback(const OnMessageCallback& callback)
    {
        _onMessageCallback = callback;
    }

    void WebSocketClient::invokeOnMessageCallback(const WebSocketMessagePtr& msg)
    {
        _onMessageCallback(msg);
    }

    std::string WebSocketClient::readyStateToString(ReadyState readyState)
    {
        switch (readyState)
        {
            case ReadyState::Open: return "OPEN";
            case ReadyState::Connecting: return "CONNECTING";
            case ReadyState::Closing: return "CLOSING";
            case ReadyState::Closed: return "CLOSED";
            default: return "UNKNOWN";
        }
    }

    bool WebSocketClient::isConnected() const
    {
        return _readyState == ReadyState::Open;
    }
} // namespace uvweb
