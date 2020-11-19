#pragma once

#include <cstdint>
#include <string>

namespace uvweb
{
    struct WebSocketCloseInfo
    {
        uint16_t code;
        std::string reason;
        bool remote;

        WebSocketCloseInfo(uint16_t c = 0, const std::string& r = std::string(), bool rem = false)
            : code(c)
            , reason(r)
            , remote(rem)
        {
            ;
        }
    };
} // namespace uvweb
