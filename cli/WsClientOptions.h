#pragma once

#include <string>
#include <vector>

struct Args
{
    std::string url;
    int msgCount = 1000000;
    bool autoroute = false;
    bool autobahn = false;
    bool shell = false;

    // Log levels
    bool traceLevel = false;
    bool debugLevel = false;
    bool infoLevel = false;
    bool warningLevel = false;
    bool errorLevel = false;
    bool criticalLevel = false;
    bool quietLevel = false;
};

bool parseOptions(int argc, char* argv[], Args& args);
void setupLogging(const Args& args);
