#pragma once
#include <string>
#include <map>
#include <uvw.hpp>
#include "http_parser.h"

namespace uvweb
{
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

    class HttpServer
    {
    public:
        HttpServer(const std::string& host, int port);
        void run();

    protected:
        virtual void processRequest(std::shared_ptr<Request> request, 
                                    Response& response);

        void writeResponse(const Response& response, uvw::TCPHandle& client);

    private:
        http_parser_settings mSettings;

        std::string _host;
        int _port;
    };
}
