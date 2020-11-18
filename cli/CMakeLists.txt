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
