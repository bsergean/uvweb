cmake_minimum_required (VERSION 3.14)
project (uvweb)

set (CMAKE_CXX_STANDARD 17)

#
# Dependencies
#
include(FetchContent)

# deflate
include(FetchContent)
FetchContent_Declare(deflate
    GIT_REPOSITORY "git@github.com:bsergean/libdeflate.git"
    GIT_TAG "master")

FetchContent_MakeAvailable(deflate)

# spdlog
include(FetchContent)
FetchContent_Declare(spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog"
    GIT_TAG "v1.8.0"
    GIT_SHALLOW 1)

FetchContent_MakeAvailable(spdlog)

# cxxopts
include(FetchContent)
FetchContent_Declare(cxxopts
    GIT_REPOSITORY "https://github.com/jarro2783/cxxopts"
    GIT_TAG "v2.1.2"
    GIT_SHALLOW 1)

FetchContent_MakeAvailable(cxxopts)

# libuv
FetchContent_Declare(
    libuv
    GIT_REPOSITORY https://github.com/libuv/libuv.git
    GIT_TAG v1.40.0
    GIT_SHALLOW 1
)
FetchContent_GetProperties(libuv)

if(NOT libuv_POPULATED)
    FetchContent_Populate(libuv)
    add_subdirectory(${libuv_SOURCE_DIR} ${libuv_BINARY_DIR})

    add_library(uv::uv-static ALIAS uv_a)
    set_target_properties(uv_a PROPERTIES EXCLUDE_FROM_ALL 1)
endif()

# uvw
FetchContent_Declare(uvw
    GIT_REPOSITORY "https://github.com/skypjack/uvw"
    GIT_TAG "v2.8.0_libuv_v1.40"
    GIT_SHALLOW 1) 
FetchContent_GetProperties(uvw)

if(NOT uvw_POPULATED)
  FetchContent_Populate(uvw)
  add_subdirectory(${uvw_SOURCE_DIR} ${uvw_BINARY_DIR})
endif()

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
  uvweb/HttpClient.cpp)

target_compile_definitions(uvweb PRIVATE SPDLOG_COMPILED_LIB=1)
target_link_libraries(uvweb spdlog uv uvw deflate)

target_include_directories(uvweb PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
  $<INSTALL_INTERFACE:include/uvweb>
)

set_target_properties(uvweb PROPERTIES PUBLIC_HEADER
  "uvweb/HttpServer.h uvweb/HttpClient.h")

# install(TARGETS uvweb
#         EXPORT uvweb
#         ARCHIVE DESTINATION lib
#         PUBLIC_HEADER DESTINATION include/
# )
# 
# # This is required to work with FetchContent
# install(EXPORT uvweb
#         FILE uvweb-config.cmake
#         NAMESPACE uvweb::
#         DESTINATION lib/cmake/uvweb)

add_subdirectory(cli)
