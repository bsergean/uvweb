#pragma once
#include <string>
#include <map>

#include <uvw.hpp>


namespace uvweb
{
    struct Request
    {
        std::map<std::string, std::string> headers;
        std::string currentHeaderName;
        std::string currentHeaderValue;
        std::string path;
        std::string body;
        std::string method;
        std::string host;
        int port;
        bool messageComplete = false;
    };

    struct Response
    {
        std::map<std::string, std::string> headers;
        int statusCode = 0;
        std::string description;
        std::string body;

        std::string currentHeaderName;
        std::string currentHeaderValue;
        bool messageComplete = false;
    };

    using OnResponseCallback = std::function<void(std::shared_ptr<Response>)>;

    class HttpClient
    {
    public:
        HttpClient();
        void fetch(const std::string& url,
                   const OnResponseCallback& onResponseCallback);

    private:
        void fetch(const sockaddr& addr,
                   const OnResponseCallback& onResponseCallback);

        void writeRequest(uvw::TCPHandle& client);

        Request mRequest;
    };
}
