
#include "http_parser.h"
#include <iostream>
#include <cstring>
#include <uvw.hpp>

#include <map>
#include <memory>
#include <functional>

using OnMessageCompleteCallback = std::function<void()>;

struct Request
{
    std::map<std::string, std::string> headers;
    std::string currentHeaderName;
    std::string currentHeaderValue;
    std::string url;
    std::string body;
    bool messageComplete = false;
};

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
    request->headers[request->currentHeaderName] = request->currentHeaderValue;
    return 0;
}

int on_message_complete(http_parser* parser)
{
    Request* request = reinterpret_cast<Request*>(parser->data);
    request->messageComplete = true;
    return 0;
}

int on_header_field(http_parser* parser, const char* at, const size_t length)
{
    Request* request = reinterpret_cast<Request*>(parser->data);
    request->currentHeaderName = std::string(at, length);
    return 0;
}

int on_header_value(http_parser* parser, const char* at, const size_t length)
{
    Request* request = reinterpret_cast<Request*>(parser->data);
    request->currentHeaderValue = std::string(at, length);
    return 0;
}

int on_body(http_parser* parser, const char* at, const size_t length)
{
    Request* request = reinterpret_cast<Request*>(parser->data);
    request->body = std::string(at, length);
    return 0;
}

int main()
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

    tcp->on<uvw::ErrorEvent>(
        [](const uvw::ErrorEvent&, uvw::TCPHandle&) { /* something went wrong */ });

    tcp->on<uvw::ListenEvent>([&settings](const uvw::ListenEvent&, uvw::TCPHandle& srv) {
        std::shared_ptr<uvw::TCPHandle> client = srv.loop().resource<uvw::TCPHandle>();
        client->once<uvw::EndEvent>(
            [](const uvw::EndEvent&, uvw::TCPHandle& client) { client.close(); });

        http_parser* parser = (http_parser*) malloc(sizeof(http_parser));
        http_parser_init(parser, HTTP_REQUEST);

        auto request = std::make_shared<Request>();
        client->data(request);

        parser->data = client->data().get();

        client->on<uvw::DataEvent>(
            [request, parser, &settings](const uvw::DataEvent& event, uvw::TCPHandle& client) {
                int nparsed = http_parser_execute(parser,
                                                  &settings,
                                                  event.data.get(),
                                                  event.length);
                if (nparsed != event.length)
                {
                    // FIXME: return 400 here
                    std::cerr << "PARSING ERROR" << std::endl;
                    return;
                }

                // Write response
                if (request->messageComplete)
                {
                    std::cout << "Message complete" << std::endl;
                    std::cout << "Message length: " << request->body.size()
                              << std::endl;

                    auto dataWrite = std::unique_ptr<char[]>(new char[]{ 'h', 'e', 'l', 'l', 'o' });
                    client.write(std::move(dataWrite), 5);
                    client.close();
                }
            });

        srv.accept(*client);
        client->read();
    });

    tcp->bind("127.0.0.1", 8600); // FIXME: bind to 0.0.0.0
    tcp->listen();

    loop->run();

    return 0;
}
