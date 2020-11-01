#
# uvw-http-server
#
add_executable(uvweb-cli)
target_sources(uvweb-cli PRIVATE options.cpp main.cpp)

target_link_libraries(uvweb-cli uvweb cxxopts)
