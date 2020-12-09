
# cxxopts
include(FetchContent)
FetchContent_Declare(cxxopts
    GIT_REPOSITORY "https://github.com/jarro2783/cxxopts"
    GIT_TAG "v2.1.2"
    GIT_SHALLOW 1)

FetchContent_MakeAvailable(cxxopts)

#
# server
#
add_executable(uvweb-server)
target_sources(uvweb-server PRIVATE ServerOptions.cpp UvwebServer.cpp)
target_link_libraries(uvweb-server uvweb cxxopts)

#
# client
#
add_executable(uvweb-client)
target_sources(uvweb-client PRIVATE ClientOptions.cpp UvwebClient.cpp)
target_link_libraries(uvweb-client uvweb cxxopts)

#
# ws client
#
add_executable(uvweb-ws-client)
target_sources(uvweb-ws-client PRIVATE WsClientOptions.cpp UvwebWsClient.cpp)
target_link_libraries(uvweb-ws-client uvweb cxxopts)

#
# pulsar client
#
add_executable(uvweb-pulsar-client)
target_sources(uvweb-pulsar-client PRIVATE PulsarOptions.cpp UvPulsarClient.cpp)
target_link_libraries(uvweb-pulsar-client uvweb cxxopts)
