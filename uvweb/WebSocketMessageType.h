
#pragma once

namespace uvweb
{
    enum class WebSocketMessageType
    {
        Message = 0,
        Open = 1,
        Close = 2,
        Error = 3,
        Ping = 4,
        Pong = 5,
        Fragment = 6
    };
}

