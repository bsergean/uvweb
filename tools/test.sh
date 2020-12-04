#!/bin/sh

test1() {
    build/cli/uvweb-ws-client --url ws://jeanserge.com:8008
}

test2() {
    PORT=8888
    ws push_server --port $PORT &
    sleep 0.5
    build/cli/uvweb-ws-client --autoroute --info --url ws://127.0.0.1:$PORT
}

test3() {
    PORT=7777
    build/cli/uvweb-server --trace --pidfile /tmp/uvweb-server.pid --port $PORT &
    sleep 0.5
    build/cli/uvweb-client --info http://127.0.0.1:$PORT

    # stop server
    kill `cat /tmp/uvweb-server.pid`
}

test4() {
    PORT=6666
    cobra run --pidfile /tmp/cobra.pid --port $PORT &
    sleep 0.5
    build/cli/uvweb-pulsar-client --info \
        --tenant public --namespace default --topic atopic \
        --url ws://127.0.0.1:$PORT \
        --msg 'hello world' --repeat 100 --delay 1

    # stop server
    kill `cat /tmp/cobra.pid`
}

test4
test3
test2
test1
