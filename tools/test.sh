#!/bin/sh

test1() {
    build/cli/uvweb-ws-client --url ws://jeanserge.com:8008
}

test2() {
    ws push_server &
    sleep 0.5
    build/cli/uvweb-ws-client --autoroute --info --url ws://127.0.0.1:8008
}

test3() {
    build/cli/uvweb-server --info --pidfile /tmp/pid --port 5678 &
    sleep 0.5
    build/cli/uvweb-client --info http://127.0.0.1:5678
    kill `cat /tmp/pid`
}

test4() {
    # cobra run
    sleep 0.5
    build/cli/uvweb-pulsar-client --debug \
        --tenant public --namespace default --topic atopic \
        --url http://127.0.0.1:8765 \
        --msg 'hello world'
}

# test4
test3
test2
test1
