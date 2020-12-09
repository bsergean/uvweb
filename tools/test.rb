# Attempt at trying to write a test script in ruby, but python might work the same

%x[ build/cli/uvweb-pulsar-client --debug \
     --tenant public --namespace default --topic atopic \
     --url ws://127.0.0.1:6666 \
     --msg 'hello world' --repeat 100 --delay 1 & ]
