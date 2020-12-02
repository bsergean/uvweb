#pragma once

#include <cstdint>
#include <string>

namespace uvweb
{
    struct WebSocketErrorInfo
    {
        uint32_t retries = 0;
        double wait_time = 0;
        int http_status = 0;
        std::string reason;
        bool decompressionError = false;

        WebSocketErrorInfo(uint32_t t = 0,
                           double w = 0,
                           int h = 0,
                           std::string r = std::string(),
                           bool d = false)
            : retries(t)
            , wait_time(w)
            , http_status(h)
            , reason(r)
            , decompressionError(d)
        {
            ;
        }
    };
} // namespace uvweb

