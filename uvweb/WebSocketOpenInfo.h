
#pragma once

#include "WebSocketHttpHeaders.h"
#include <cstdint>
#include <string>

namespace uvweb
{
    struct WebSocketOpenInfo
    {
        std::string uri;
        WebSocketHttpHeaders headers;
        std::string protocol;

        WebSocketOpenInfo(const std::string& u = std::string(),
                          const WebSocketHttpHeaders& h = WebSocketHttpHeaders(),
                          const std::string& p = std::string())
            : uri(u)
            , headers(h)
            , protocol(p)
        {
            ;
        }
    };
} // namespace uvweb
