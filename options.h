#pragma once

#include <string>

struct Args
{
    std::string host;
    int port;

    // Log levels
    bool traceLevel = false;
    bool debugLevel = false;
    bool infoLevel = false;
    bool warningLevel = false;
    bool errorLevel = false;
    bool criticalLevel = false;
    bool quietLevel = false;
};

bool parseOptions(int argc, char * argv[], Args& args);
