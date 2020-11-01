#pragma once
#include <string>
#include <map>


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

    private:
        std::string _host;
        int _port;
    };
}
