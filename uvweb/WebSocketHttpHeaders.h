#pragma once

#include "StrCaseCompare.h"

#include <map>
#include <memory>
#include <string>

namespace uvweb
{
    using WebSocketHttpHeaders = std::map<std::string, std::string, CaseInsensitiveLess>;
}
