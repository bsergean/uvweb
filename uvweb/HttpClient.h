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

    class HttpClient
    {
    public:
        HttpClient();
        void fetch(const std::string& url);

    private:
    };
}
