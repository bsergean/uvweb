cmake_minimum_required (VERSION 3.14)
project (uvw-server)

set (CMAKE_CXX_STANDARD 17)

#
# Dependencies
#
include(FetchContent)

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
add_executable(uvw-server 
  main.cpp 
  http_parser.c)

target_link_libraries(uvw-server uv uvw)
