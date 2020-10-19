cmake_minimum_required (VERSION 3.14)
project (uvw-server)

set (CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(uv
    GIT_REPOSITORY "https://github.com/libuv/libuv"
    GIT_TAG "v1.35.0") 

include(FetchContent)
FetchContent_Declare(uvw
    GIT_REPOSITORY "https://github.com/skypjack/uvw"
    GIT_TAG "v2.4.0_libuv-v1.35") 

FetchContent_MakeAvailable(uv uvw)

add_executable(uvw-server 
  main.cpp 
  http_parser.c)

target_link_libraries(uvw-server uv uvw)
