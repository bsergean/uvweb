


#include "WebSocketClient.h"

#include "UrlParser.h"
#include "Utf8Validator.h"
#include "gzip.h"
#include "http_parser.h"
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>
#include <chrono>
#include <uvw.hpp>

namespace uvweb
{
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

    void writeHandshakeRequest(const Request& request, uvw::TCPHandle& client)
    {
        // Write the request to the socket
        std::stringstream ss;
        ss << request.method;
        ss << " ";
        ss << request.path;
        ss << " ";
        ss << "HTTP/1.1\r\n";

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
        ss << "Host: " << request.host << "\r\n";
        ss << "Upgrade: websocket\r\n";
        ss << "Connection: Upgrade\r\n";
        ss << "Sec-WebSocket-Version: 13\r\n";
        ss << "Sec-WebSocket-Key: " << secWebSocketKey << "\r\n";

        for (auto&& it : request.headers)
        {
            ss << it.first << ": " << it.second << "\r\n";
        }
        ss << "\r\n";

        auto str = ss.str();
        spdlog::debug("Client request: {}", str);
        auto buff = std::make_unique<char[]>(str.length());
        std::copy_n(str.c_str(), str.length(), buff.get());

        client.write(std::move(buff), str.length());
    }

    constexpr size_t WebSocketClient::kChunkSize;

    WebSocketClient::WebSocketClient()
        : _useMask(true)
        , _readyState(ReadyState::Closed)
        , _closeCode(WebSocketCloseConstants::kInternalErrorCode)
    {
        ;
    }

    void WebSocketClient::connect(const std::string& url, const OnMessageCallback& callback)
    {
        std::string protocol, host, path, query;
        int port;

        if (!UrlParser::parse(url, protocol, host, path, query, port))
        {
            std::stringstream ss;
            ss << "Could not parse url: '" << url << "'";
            spdlog::error(ss.str());
            return;
        }

        auto loop = uvw::Loop::getDefault();

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

        auto client = loop->resource<uvw::TCPHandle>();

        // Register http parser callbacks
        http_parser_settings settings;
        memset(&settings, 0, sizeof(settings));
        settings.on_message_begin = on_message_begin;
        settings.on_status = on_status;
        settings.on_headers_complete = on_headers_complete;
        settings.on_message_complete = on_message_complete;
        settings.on_header_field = on_header_field;
        settings.on_header_value = on_header_value;
        settings.on_body = on_body;

        http_parser* parser = (http_parser*) malloc(sizeof(http_parser));
        http_parser_init(parser, HTTP_RESPONSE);

        auto response = std::make_shared<Response>();
        client->data(response);

        parser->data = client->data().get();

        Request request;
        request.method = "GET";
        request.path = path;
        request.host = host;

        // On Error
        client->on<uvw::ErrorEvent>([&host, &port](const uvw::ErrorEvent& errorEvent,
                                                   uvw::TCPHandle&) {
            spdlog::error("Connection to {} on port {} failed : {}", host, port, errorEvent.name());
        });

        // On connect
        client->once<uvw::ConnectEvent>(
            [&request](const uvw::ConnectEvent&, uvw::TCPHandle& client) {
                writeHandshakeRequest(request, client);
            });

        client->on<uvw::DataEvent>([this, response, parser, &settings, callback](
                                    const uvw::DataEvent& event, uvw::TCPHandle& client) {
            int nparsed = http_parser_execute(parser, &settings,
                                              event.data.get(), event.length);

            if (nparsed != event.length)
            {
                std::stringstream ss;
                ss << "HTTP Parsing Error: "
                   << "description: " << http_errno_description(HTTP_PARSER_ERRNO(parser))
                   << " error name " << http_errno_name(HTTP_PARSER_ERRNO(parser)) << " nparsed "
                   << nparsed << " event.length " << event.length;
                spdlog::error(ss.str());
                return;
            }

            // Write response
            if (response->messageComplete && parser->upgrade)
            {
                spdlog::info("HTTP Upgrade, status code: {}", response->statusCode);

                // FIXME: missing validation of WebSocket Key header

                setReadyState(ReadyState::Open);

                callback(std::make_unique<WebSocketMessage>(
                    WebSocketMessageType::Open,
                    "",
                    0,
                    WebSocketErrorInfo(),
                    // WebSocketOpenInfo(status.uri, response.headers, status.protocol),
                    WebSocketOpenInfo(response->uri, response->headers, response->protocol),
                    WebSocketCloseInfo()));

                // emit connected callback
                // client.close();
            }
        });

        client->connect(*addr->ai_addr);

        client->read();
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

    void WebSocketClient::close(uint16_t code, const std::string& reason)
    {
        // FIXME no-op
    }

    bool WebSocketClient::sendData(wsheader_type::opcode_type type,
                                   const std::string& message)
    {
        if (_readyState != ReadyState::Open && _readyState != ReadyState::Closing)
        {
            return false;
        }

        size_t wireSize = message.size();
        auto message_begin = message.cbegin();
        auto message_end = message.cend();

        _txbuf.reserve(wireSize);

        bool success = true;
        const bool compress = false; // FIXME not supported yet

        // Common case for most message. No fragmentation required.
        if (wireSize < kChunkSize)
        {
            success = sendFragment(type, true, message_begin, message_end, compress);
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
                if (!sendFragment(opcodeType, fin, begin, end, compress))
                {
                    return false;
                }

                begin += kChunkSize;
            }
        }

        return true;
    }

    bool WebSocketClient::sendFragment(wsheader_type::opcode_type type,
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

        // _txbuf will keep growing until it can be transmitted over the socket:
        appendToSendBuffer(header, message_begin, message_end, message_size, masking_key);

        // Now actually send this data
        return sendOnSocket();
    }

    void WebSocketClient::appendToSendBuffer(const std::vector<uint8_t>& header,
                                             std::string::const_iterator begin,
                                             std::string::const_iterator end,
                                             uint64_t message_size,
                                             uint8_t masking_key[4])
    {
        _txbuf.insert(_txbuf.end(), header.begin(), header.end());
        _txbuf.insert(_txbuf.end(), begin, end);

        if (_useMask)
        {
            for (size_t i = 0; i != (size_t) message_size; ++i)
            {
                *(_txbuf.end() - (size_t) message_size + i) ^= masking_key[i & 0x3];
            }
        }
    }

    bool WebSocketClient::sendOnSocket()
    {
#if 0
        while (_txbuf.size())
        {
            ssize_t ret = 0;
            {
                ret = _socket->send((char*) &_txbuf[0], _txbuf.size());
            }

            if (ret < 0 && Socket::isWaitNeeded())
            {
                break;
            }
            else if (ret <= 0)
            {
                closeSocket();
                setReadyState(ReadyState::Closed);
                return false;
            }
            else
            {
                _txbuf.erase(_txbuf.begin(), _txbuf.begin() + ret);
            }
        }
#endif
        spdlog::debug("sendOnSocket");

        return true;
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
        _readyState = readyState;
    }
} // namespace uvweb
