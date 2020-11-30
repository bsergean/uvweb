
#include "HttpServer.h"

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

    int on_url(http_parser* parser, const char* at, const size_t length)
    {
        Request* request = reinterpret_cast<Request*>(parser->data);
        request->url = std::string(at, length);
        return 0;
    }

    int on_headers_complete(http_parser* parser)
    {
        Request* request = reinterpret_cast<Request*>(parser->data);

        spdlog::debug("All headers parsed");
        for (const auto& it : request->headers)
        {
            spdlog::debug("{}: {}", it.first, it.second);
        }

        return 0;
    }

    int on_message_complete(http_parser* parser)
    {
        Request* request = reinterpret_cast<Request*>(parser->data);
        request->messageComplete = true;

        if (request->headers["Content-Encoding"] == "gzip")
        {
            spdlog::debug("decoding gzipped body");

            std::string decompressedBody;
            if (!gzipDecompress(request->body, decompressedBody))
            {
                return 1;
            }
            request->body = decompressedBody;
        }

        spdlog::debug("body value {}", request->body);
        return 0;
    }

    int on_header_field(http_parser* parser, const char* at, const size_t length)
    {
        Request* request = reinterpret_cast<Request*>(parser->data);
        request->currentHeaderName = std::string(at, length);

        spdlog::debug("on header field {}", request->currentHeaderName);
        return 0;
    }

    int on_header_value(http_parser* parser, const char* at, const size_t length)
    {
        Request* request = reinterpret_cast<Request*>(parser->data);
        request->currentHeaderValue = std::string(at, length);

        request->headers[request->currentHeaderName] = request->currentHeaderValue;

        spdlog::debug("on header value {}", request->currentHeaderValue);
        return 0;
    }

    int on_body(http_parser* parser, const char* at, const size_t length)
    {
        Request* request = reinterpret_cast<Request*>(parser->data);
        auto body = std::string(at, length);
        request->body += body;

        spdlog::debug("on body {}", body);
        return 0;
    }

    void writeResponse(const Response& response, uvw::TCPHandle& client)
    {
        // Write the response to the socket
        std::stringstream ss;
        ss << "HTTP/1.1 ";
        ss << response.statusCode;
        ss << " ";
        ss << response.description;
        ss << "\r\n";

        // Write headers
        ss << "Content-Length: " << response.body.size() << "\r\n";
        ss << "Server: uvw-server"
           << "\r\n";
        for (auto&& it : response.headers)
        {
            ss << it.first << ": " << it.second << "\r\n";
        }
        ss << "\r\n";
        ss << response.body;

        auto str = ss.str();
        spdlog::debug("Server response: {}", str);
        auto buff = std::make_unique<char[]>(str.length());
        std::copy_n(str.c_str(), str.length(), buff.get());

        client.write(std::move(buff), str.length());
    }

    HttpServer::HttpServer(const std::string& host, int port)
        : _host(host)
        , _port(port)
    {
        ;
    }

    void HttpServer::run()
    {
        auto loop = uvw::Loop::getDefault();
        auto tcp = loop->resource<uvw::TCPHandle>();

        // Register http parser callbacks
        http_parser_settings settings;
        memset(&settings, 0, sizeof(settings));
        settings.on_message_begin = on_message_begin;
        settings.on_url = on_url;
        settings.on_headers_complete = on_headers_complete;
        settings.on_message_complete = on_message_complete;
        settings.on_header_field = on_header_field;
        settings.on_header_value = on_header_value;
        settings.on_body = on_body;

        tcp->on<uvw::ErrorEvent>([](const uvw::ErrorEvent& errorEvent, uvw::TCPHandle&) {
            /* something went wrong */
            spdlog::error("Listen socket error {}", errorEvent.name());
        });

        tcp->on<uvw::ListenEvent>([&settings](const uvw::ListenEvent&, uvw::TCPHandle& srv) {
            std::shared_ptr<uvw::TCPHandle> client = srv.loop().resource<uvw::TCPHandle>();
            client->once<uvw::EndEvent>(
                [](const uvw::EndEvent&, uvw::TCPHandle& client) { client.close(); });

            client->on<uvw::ErrorEvent>(
                [](const uvw::ErrorEvent& errorEvent, uvw::TCPHandle& client) {
                    /* something went wrong */
                    spdlog::error("socket error {}", errorEvent.name());
                    client.close();
                });

            http_parser* parser = (http_parser*) malloc(sizeof(http_parser));
            http_parser_init(parser, HTTP_REQUEST);

            auto request = std::make_shared<Request>();
            client->data(request);

            parser->data = client->data().get();

            client->on<uvw::DataEvent>(
                [request, parser, &settings](const uvw::DataEvent& event, uvw::TCPHandle& client) {
                    auto data = std::string(event.data.get(), event.length);
                    spdlog::trace("DataEvent: {}", data);

                    int nparsed =
                        http_parser_execute(parser, &settings, event.data.get(), event.length);

                    if (nparsed != event.length)
                    {
                        std::stringstream ss;
                        ss << "HTTP Parsing Error: "
                           << "description: " << http_errno_description(HTTP_PARSER_ERRNO(parser))
                           << " error name " << http_errno_name(HTTP_PARSER_ERRNO(parser))
                           << " nparsed " << nparsed << " event.length " << event.length;

                        Response response;
                        response.statusCode = 400;
                        response.description = "KO";
                        response.body = ss.str();

                        writeResponse(response, client);
                        return;
                    }

                    // Write response
                    if (request->messageComplete)
                    {
                        Response response;
                        response.statusCode = 200;
                        response.description = "OK";
                        response.body = "OK";

                        writeResponse(response, client);
                    }
                });

            srv.accept(*client);
            client->read();
        });

        spdlog::info("Listening on {}:{}", _host, _port);

        tcp->bind(_host, _port);
        tcp->listen();
    }
} // namespace uvweb
