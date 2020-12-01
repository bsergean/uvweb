# Build time
FROM ubuntu:focal as build

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update

RUN apt-get -y install g++ libz-dev make ninja-build cmake python git

COPY . /opt
WORKDIR /opt

#
# To use the container interactively for debugging/building
# 1. Build with
#    CMD ["ls"]
# 2. Run with
#    docker run --entrypoint sh -it docker-game-eng-dev.addsrv.com/ws:9.10.6 
#

RUN ["mkdir", "build"]
RUN ["make", "full_build_release"]
