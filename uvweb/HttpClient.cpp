

#include "HttpClient.h"

#include "UrlParser.h"
#include "gzip.h"
#include "http_parser.h"
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
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
            SPDLOG_DEBUG("{}: {}", it.first, it.second);
        }

        return 0;
    }

    int on_message_complete(http_parser* parser)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        response->messageComplete = true;

        if (response->headers["Content-Encoding"] == "gzip")
        {
            SPDLOG_DEBUG("decoding gzipped body");

            std::string decompressedBody;
            if (!gzipDecompress(response->body, decompressedBody))
            {
                return 1;
            }
            response->body = decompressedBody;
        }

        SPDLOG_DEBUG("body value {}", response->body);
        return 0;
    }

    int on_header_field(http_parser* parser, const char* at, const size_t length)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        response->currentHeaderName = std::string(at, length);

        SPDLOG_DEBUG("on header field {}", response->currentHeaderName);
        return 0;
    }

    int on_header_value(http_parser* parser, const char* at, const size_t length)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        response->currentHeaderValue = std::string(at, length);

        response->headers[response->currentHeaderName] = response->currentHeaderValue;

        SPDLOG_DEBUG("on header value {}", response->currentHeaderValue);
        return 0;
    }

    int on_body(http_parser* parser, const char* at, const size_t length)
    {
        Response* response = reinterpret_cast<Response*>(parser->data);
        auto body = std::string(at, length);
        response->body += body;

        SPDLOG_DEBUG("on body {}", body);
        return 0;
    }

    HttpClient::HttpClient()
    {
        ;
    }

    void HttpClient::writeRequest(uvw::TCPHandle& client)
    {
        // Write the request to the socket
        std::stringstream ss;
        ss << mRequest.method;
        ss << " ";
        ss << mRequest.path;
        ss << " ";
        ss << "HTTP/1.1\r\n";

        // Write headers
        if (mRequest.method != "GET" && mRequest.method != "HEAD")
        {
            ss << "Content-Length: " << mRequest.body.size() << "\r\n";
        }

        // FIXME: only write those default headers if no user supplied are presents
        ss << "Host: " << mRequest.host << "\r\n";
        ss << "Accept: */*"
           << "\r\n";
        ss << "Accept-Encoding: gzip"
           << "\r\n";
        ss << "User-Agent: uvweb-client"
           << "\r\n";

        for (auto&& it : mRequest.headers)
        {
            ss << it.first << ": " << it.second << "\r\n";
        }
        ss << "\r\n";

        if (mRequest.method != "GET" && mRequest.method != "HEAD")
        {
            ss << mRequest.body;
            ss << "\r\n";
        }

        auto str = ss.str();
        SPDLOG_DEBUG("Client request: {}", str);
        auto buff = std::make_unique<char[]>(str.length());
        std::copy_n(str.c_str(), str.length(), buff.get());

        client.write(std::move(buff), str.length());
    }

    void HttpClient::fetch(
        const std::string& url,
        const OnResponseCallback& onResponseCallback)
    {
        std::string protocol, host, path, query;
        int port;

        if (!UrlParser::parse(url, protocol, host, path, query, port))
        {
            std::stringstream ss;
            ss << "Could not parse url: '" << url << "'";
            SPDLOG_ERROR(ss.str());
            return;
        }

        mRequest.method = "GET";
        mRequest.path = path;
        mRequest.host = host;
        mRequest.port = port;

        auto loop = uvw::Loop::getDefault();

        // async DNS lookup
        auto request = loop->resource<uvw::GetAddrInfoReq>();

        request->on<uvw::ErrorEvent>([host, port](const uvw::ErrorEvent& errorEvent, auto& /* handle */) {
            // FIXME emit onResponseCallback
            SPDLOG_ERROR(
                "Connection to {}:{} failed : {}", host, port, errorEvent.name());
        });

        request->on<uvw::AddrInfoEvent>([this, onResponseCallback](const auto& addrInfoEvent, auto& /* handle */) {
            sockaddr addr = *(addrInfoEvent.data)->ai_addr;
            fetch(addr, onResponseCallback);
        });

        request->addrInfo(host, std::to_string(port));
    }

    void HttpClient::fetch(
        const sockaddr& addr,
        const OnResponseCallback& onResponseCallback)
    {
        auto loop = uvw::Loop::getDefault();
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

        // On Error
        client->on<uvw::ErrorEvent>([this](const uvw::ErrorEvent& errorEvent,
                                           uvw::TCPHandle&) {
            // FIXME emit onResponseCallback
            SPDLOG_ERROR("Connection to {} on port {} failed : {}", mRequest.host, mRequest.port, errorEvent.name());
        });

        // On connect
        client->once<uvw::ConnectEvent>(
            [this](const uvw::ConnectEvent&, uvw::TCPHandle& client) {
                writeRequest(client);
            });

        client->on<uvw::DataEvent>([response, parser, &settings, onResponseCallback](const uvw::DataEvent& event,
                                                                 uvw::TCPHandle& client) {
            int nparsed = http_parser_execute(parser, &settings, event.data.get(), event.length);

            if (nparsed != event.length)
            {
                std::stringstream ss;
                ss << "HTTP Parsing Error: "
                   << "description: " << http_errno_description(HTTP_PARSER_ERRNO(parser))
                   << " error name " << http_errno_name(HTTP_PARSER_ERRNO(parser)) << " nparsed "
                   << nparsed << " event.length " << event.length;
                SPDLOG_ERROR(ss.str());
                return;
            }

            // Write response
            if (response->messageComplete)
            {
                SPDLOG_INFO("Message complete, status code: {}", response->statusCode);
                onResponseCallback(response);
                client.close();
            }
        });

        client->connect(addr);

        client->read();
        loop->run();
    }
} // namespace uvweb
