cmake_minimum_required (VERSION 3.14)
project (uvweb)

set (CMAKE_CXX_STANDARD 17)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

#
# uvw-http-server
#
add_library(uvweb)
add_library(uvweb::uvweb ALIAS uvweb)

target_sources(uvweb PRIVATE 
  uvweb/gzip.cpp
  uvweb/http_parser.c 
  uvweb/UrlParser.cpp
  uvweb/HttpServer.cpp
  uvweb/HttpClient.cpp
  uvweb/WebSocketClient.cpp
  uvweb/WebSocketCloseConstants.cpp
  uvweb/StrCaseCompare.cpp
  uvweb/Base64.cpp
  uvweb/chromiumbase64.c
  uvweb/PulsarClient.cpp
)

target_link_libraries(uvweb ${CONAN_LIBS})

target_include_directories(uvweb PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
  $<INSTALL_INTERFACE:include/uvweb>
)

set_target_properties(uvweb PROPERTIES PUBLIC_HEADER
  "uvweb/HttpServer.h uvweb/HttpClient.h")

add_subdirectory(cli)
