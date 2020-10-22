
#include "http_parser.h"
#include "options.h"
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <uvw.hpp>
#include <zlib.h>


bool gzipDecompress(const std::string& in, std::string& out)
{
    z_stream inflateState;
    memset(&inflateState, 0, sizeof(inflateState));

    inflateState.zalloc = Z_NULL;
    inflateState.zfree = Z_NULL;
    inflateState.opaque = Z_NULL;
    inflateState.avail_in = 0;
    inflateState.next_in = Z_NULL;

    if (inflateInit2(&inflateState, 16 + MAX_WBITS) != Z_OK)
    {
        return false;
    }

    inflateState.avail_in = (uInt) in.size();
    inflateState.next_in = (unsigned char*) (const_cast<char*>(in.data()));

    const int kBufferSize = 1 << 14;
    std::array<unsigned char, kBufferSize> compressBuffer;

    do
    {
        inflateState.avail_out = (uInt) kBufferSize;
        inflateState.next_out = &compressBuffer.front();

        int ret = inflate(&inflateState, Z_SYNC_FLUSH);

        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
        {
            inflateEnd(&inflateState);
            return false;
        }

        out.append(reinterpret_cast<char*>(&compressBuffer.front()),
                   kBufferSize - inflateState.avail_out);
    } while (inflateState.avail_out == 0);

    inflateEnd(&inflateState);
    return true;
}

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

struct Response
{
    std::map<std::string, std::string> headers;
    int statusCode = 200;
    std::string description;
    std::string body;
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

    if (request->headers["Content-Encoding"] == "gzip")
    {
        spdlog::debug("decoding gzipped body");

        std::string decompressedBody;
        if (!gzipDecompress(body, decompressedBody))
        {
            return 1;
        }
        body = decompressedBody;
    }

    spdlog::debug("body value {}", body);

    request->body = body;
    return 0;
}

void writeResponse(const Response& response,
                   uvw::TCPHandle& client)
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
    auto buff = std::make_unique<char[]>(str.length() + 1);
    std::copy_n(str.c_str(), str.length() + 1, buff.get());

    client.write(std::move(buff), str.length() + 1);
    client.close();
}

int main(int argc, char* argv[])
{
    Args args;

    if (!parseOptions(argc, argv, args))
    {
        return 1;
    }

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

        client->on<uvw::DataEvent>([request, parser, &settings](const uvw::DataEvent& event,
                                                                uvw::TCPHandle& client) {
            int nparsed = http_parser_execute(parser, &settings, event.data.get(), event.length);
            if (nparsed != event.length)
            {
                Response response;
                response.statusCode = 400;
                response.description = "KO";
                response.body = "HTTP Parsing Error";

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

    tcp->bind("127.0.0.1", 8600); // FIXME: bind to 0.0.0.0
    tcp->listen();

    loop->run();

    return 0;
}
