
#include "PulsarOptions.h"

#include <cxxopts.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

void setupLogging(const Args& args)
{
    // We want all logs to go to stderr
    auto logger = spdlog::stderr_color_mt("stderr");
    spdlog::set_default_logger(logger);

    // Default level is warning.
    spdlog::set_level(spdlog::level::warn);

    if (args.traceLevel)
    {
        spdlog::set_level(spdlog::level::trace);
    }
    if (args.debugLevel)
    {
        spdlog::set_level(spdlog::level::debug);
    }
    if (args.infoLevel)
    {
        spdlog::set_level(spdlog::level::info);
    }
    if (args.warningLevel)
    {
        spdlog::set_level(spdlog::level::warn);
    }
    if (args.errorLevel)
    {
        spdlog::set_level(spdlog::level::err);
    }
    if (args.criticalLevel)
    {
        spdlog::set_level(spdlog::level::critical);
    }
    if (args.quietLevel)
    {
        spdlog::set_level(spdlog::level::off);
    }
}

bool parseOptions(int argc, char* argv[], Args& args)
{
    //
    // Option parsing
    //
    cxxopts::Options options("uvweb-client", "A toy HTTP client");

    // clang-format off
    options.add_options()
        ( "u,url", "Param url", cxxopts::value<std::string>() )
        ( "m,msg", "msg", cxxopts::value<std::string>() )
        ( "tenant", "tenant", cxxopts::value<std::string>() )
        ( "n,namespace", "Namespace", cxxopts::value<std::string>() )
        ( "t,topic", "Topic", cxxopts::value<std::string>() )
        ( "repeat", "Repeat", cxxopts::value<int>() )
        ( "h,help", "Print usage" )

        // Log levels
        ( "trace", "Trace level", cxxopts::value<bool>()->default_value( "false" ) )
        ( "debug", "Debug level", cxxopts::value<bool>()->default_value( "false" ) )
        ( "info", "Info level", cxxopts::value<bool>()->default_value( "false" ) )
        ( "warning", "Warning level", cxxopts::value<bool>()->default_value( "false" ) )
        ( "error", "Error level", cxxopts::value<bool>()->default_value( "false" ) )
        ( "critical", "Critical log", cxxopts::value<bool>()->default_value( "false" ) )
        ( "quiet", "No log", cxxopts::value<bool>()->default_value( "false" ) )
    ;

    try
    {
        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            return false;
        }

        if (result.count("url") == 0)
        {
            std::cerr << "Error: an url is required." << std::endl;
            return false;
        }

        if (result.count("tenant") == 0)
        {
            std::cerr << "Error: a tenant is required." << std::endl;
            return false;
        }

        if (result.count("namespace") == 0)
        {
            std::cerr << "Error: a namespace is required." << std::endl;
            return false;
        }

        if (result.count("topic") == 0)
        {
            std::cerr << "Error: a topic is required." << std::endl;
            return false;
        }

        if (result.count("msg") == 0)
        {
            std::cerr << "Error: a msg is required." << std::endl;
            return false;
        }

        args.url = result["url"].as<std::string>();
        args.tenant = result["tenant"].as<std::string>();
        args.nameSpace = result["namespace"].as<std::string>();

        auto msg = result["msg"].as<std::string>();
        auto topic = result["topic"].as<std::string>();

        if (result.count("repeat") > 0)
        {
            auto count = result["repeat"].as<int>();
            for (int i = 0; i < count ; ++i)
            {
                args.messages.push_back(msg);
                args.topics.push_back(topic);
            }
        }
        else
        {
            args.messages.push_back(msg);
            args.topics.push_back(topic);
        }

        args.traceLevel = result["trace"].as<bool>();
        args.debugLevel = result["debug"].as<bool>();
        args.infoLevel = result["info"].as<bool>();
        args.warningLevel = result["warning"].as<bool>();
        args.errorLevel = result["error"].as<bool>();
        args.criticalLevel = result["critical"].as<bool>();
        args.quietLevel = result["quiet"].as<bool>();
    }
    catch (const cxxopts::OptionException& e)
    {
        std::cerr << e.what() << std::endl;
        return false;
    }

    setupLogging(args);

    return true;
}
