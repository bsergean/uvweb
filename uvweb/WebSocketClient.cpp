


#include "WebSocketClient.h"

#include "UrlParser.h"
#include "Utf8Validator.h"
#include "gzip.h"
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>
#include <chrono>

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

    constexpr size_t WebSocketClient::kChunkSize;

    WebSocketClient::WebSocketClient()
        : _useMask(true)
        , _readyState(ReadyState::Closed)
        , _closeCode(WebSocketCloseConstants::kInternalErrorCode)
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

        mRequest.method = "GET";
        mRequest.path = path;
        mRequest.host = host;

        // On Error
        mClient->on<uvw::ErrorEvent>([&host, &port](const uvw::ErrorEvent& errorEvent,
                                                    uvw::TCPHandle&) {
            spdlog::error("Connection to {} on port {} failed : {}", host, port, errorEvent.name());
        });

        // On connect
        mClient->once<uvw::ConnectEvent>(
            [this](const uvw::ConnectEvent&, uvw::TCPHandle&) {
                if (!writeHandshakeRequest())
                {
                    spdlog::error("Error sending handshake");
                }
            });

        mClient->on<uvw::DataEvent>([this, response, callback](
                                     const uvw::DataEvent& event, uvw::TCPHandle& client) {
            int nparsed = http_parser_execute(mHttpParser.get(), &mSettings,
                                              event.data.get(), event.length);

            if (nparsed != event.length)
            {
                std::stringstream ss;
                ss << "HTTP Parsing Error: "
                   << "description: " << http_errno_description(HTTP_PARSER_ERRNO(mHttpParser))
                   << " error name " << http_errno_name(HTTP_PARSER_ERRNO(mHttpParser)) << " nparsed "
                   << nparsed << " event.length " << event.length;
                spdlog::error(ss.str());
                return;
            }

            // Write response
            if (response->messageComplete && mHttpParser->upgrade)
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

        mClient->connect(*addr->ai_addr);
        mClient->read();
    }

    bool WebSocketClient::writeHandshakeRequest()
    {
        // Write the request to the socket
        std::stringstream ss;
        ss << mRequest.method;
        ss << " ";
        ss << mRequest.path;
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
        ss << "Host: " << mRequest.host << "\r\n";
        ss << "Upgrade: websocket\r\n";
        ss << "Connection: Upgrade\r\n";
        ss << "Sec-WebSocket-Version: 13\r\n";
        ss << "Sec-WebSocket-Key: " << secWebSocketKey << "\r\n";

        for (auto&& it : mRequest.headers)
        {
            ss << it.first << ": " << it.second << "\r\n";
        }
        ss << "\r\n";

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

        bool success = true;
        const bool compress = false; // FIXME not supported yet

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
