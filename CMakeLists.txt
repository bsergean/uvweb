cmake_minimum_required (VERSION 3.14)
project (uvw-server)

set (CMAKE_CXX_STANDARD 17)

#
# Dependencies
#
include(FetchContent)

# spdlog
include(FetchContent)
FetchContent_Declare(spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog"
    GIT_TAG "v1.8.0")

FetchContent_MakeAvailable(spdlog)

# cxxopts
include(FetchContent)
FetchContent_Declare(cxxopts
    GIT_REPOSITORY "https://github.com/jarro2783/cxxopts"
    GIT_TAG "v2.1.2")

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
add_executable(uvw-server 
  main.cpp 
  options.cpp
  http_parser.c)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
add_definitions(-DSPDLOG_COMPILED_LIB=1)

target_link_libraries(uvw-server cxxopts spdlog uv uvw ${ZLIB_LIBRARIES})
