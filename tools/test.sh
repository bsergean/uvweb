#!/bin/sh

test1() {
    build/cli/uvweb-ws-client --url ws://jeanserge.com:8008
}

test2() {
    ws push_server &
    sleep 0.5
    build/cli/uvweb-ws-client --autoroute --info --url ws://127.0.0.1:8008
}

test1
test2
