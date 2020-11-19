#pragma once

#include "WebSocketCloseInfo.h"
#include "WebSocketErrorInfo.h"
#include "WebSocketMessageType.h"
#include "WebSocketOpenInfo.h"
#include <memory>
#include <string>
#include <thread>

namespace uvweb
{
    struct WebSocketMessage
    {
        WebSocketMessageType type;
        const std::string& str;
        size_t wireSize;
        WebSocketErrorInfo errorInfo;
        WebSocketOpenInfo openInfo;
        WebSocketCloseInfo closeInfo;
        bool binary;

        WebSocketMessage(WebSocketMessageType t,
                         const std::string& s,
                         size_t w,
                         WebSocketErrorInfo e,
                         WebSocketOpenInfo o,
                         WebSocketCloseInfo c,
                         bool b = false)
            : type(t)
            , str(s)
            , wireSize(w)
            , errorInfo(e)
            , openInfo(o)
            , closeInfo(c)
            , binary(b)
        {
            ;
        }
    };

    using WebSocketMessagePtr = std::unique_ptr<WebSocketMessage>;
} // namespace uvweb

