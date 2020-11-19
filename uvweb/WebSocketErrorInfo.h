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
    };
} // namespace uvweb

