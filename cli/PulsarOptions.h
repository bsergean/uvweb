
#pragma once

#include <string>
#include <vector>

struct Args
{
    std::string url;
    std::string tenant;
    std::string nameSpace;
    std::string topic;
    std::string msg;

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
