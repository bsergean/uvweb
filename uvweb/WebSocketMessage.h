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
        bool binary;
        std::unique_ptr<WebSocketErrorInfo> errorInfo;
        std::unique_ptr<WebSocketOpenInfo> openInfo;
        std::unique_ptr<WebSocketCloseInfo> closeInfo;

        WebSocketMessage(WebSocketMessageType t,
                         const std::string& s,
                         size_t w,
                         bool b,
                         std::unique_ptr<WebSocketErrorInfo> e,
                         std::unique_ptr<WebSocketOpenInfo> o,
                         std::unique_ptr<WebSocketCloseInfo> c)
            : type(t)
            , str(s)
            , wireSize(w)
            , binary(b)
            , errorInfo(std::move(e))
            , openInfo(std::move(o))
            , closeInfo(std::move(c))
        {
            ;
        }
    };

    using WebSocketMessagePtr = std::unique_ptr<WebSocketMessage>;
} // namespace uvweb

