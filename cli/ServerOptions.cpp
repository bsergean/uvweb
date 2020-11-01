#include "ServerOptions.h"

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
    cxxopts::Options options("uvweb-server", "A toy HTTP server");

    // clang-format off
    options.add_options()
        ("host", "Host to bind to", cxxopts::value<std::string>()->default_value( "127.0.0.1"))
        ( "port", "Port", cxxopts::value<int>()->default_value("8080"))
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
    // clang-format on

    try
    {
        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help() << std::endl;
            return false;
        }

        args.host = result["host"].as<std::string>();
        args.port = result["port"].as<int>();

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
