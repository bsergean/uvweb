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

# Publish messages
test4() {
    PORT=6666
    # cobra run --pidfile /tmp/cobra.pid --no_stats --port $PORT &
    sleep 0.5
    build/cli/uvweb-pulsar-client --info \
        --tenant public --namespace default --topic atopic \
        --url ws://jeanserge.com:6666 \
        --msg 'hello world how are you' --repeat 100 --delay 1

    # stop cobra server
    # kill `cat /tmp/cobra.pid`
}

# Publish and Subscribe
test5() {
    # killall uvweb-pulsar-client
    # cobra run --pidfile /tmp/cobra.pid --no_stats --port $PORT &
    PIDFILE=/tmp/pulsar-subscriber.pid

    HOST=127.0.0.1
    HOST=jeanserge.com

    build/cli/uvweb-pulsar-client --debug \
        --tenant public --namespace default --topic atopic \
        --subscribe --subscription sub --max_messages 10 \
        --url ws://$HOST:6666 &

    sleep 0.3

    build/cli/uvweb-pulsar-client --info \
        --tenant public --namespace default --topic atopic \
        --url ws://$HOST:6666 \
        --msg 'hello world' --repeat 10 --delay 1

    # stop cobra server
    # kill `cat /tmp/cobra.pid`
}

test5
test4
test3
test2
test1
