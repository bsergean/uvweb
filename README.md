# uvweb

Http, WebSocket code based on libuv, uvw (libuv C++ binding) and http_parser.

## Pulsar client

### Publisher

```
#include <uvweb/PulsarClient.h>
#include <uvw.hpp>
#include <iostream>

int main()
{
    std::string url("ws://localhost:8080");

    // A queue is used to hold published messages until they are delivered.
    // Its size is bounded so programs do not run out of memory
    size_t maxQueueSize = 1000;

    uvweb::PulsarClient pulsarClient(url, maxQueueSize);

    std::string message("hello world");
    std::string tenant("public");
    std::string namespace("default");
    std::string topic("topic_name");

    pulsarClient.publish(
        message,
        tenant,
        nameSpace,
        topic,
        [](bool success, const std::string& context, const std::string& messageId) {
            std::string successString = (success) ? "true" : "false";
            std::cout << "Publish successful: " << successString 
                      << " context " << context
                      << " message id: " << messageId << std::endl;
        });

    auto loop = uvw::Loop::getDefault();
    loop->run();

    return 0;
}
```

### Consumer

```
#include <uvweb/PulsarClient.h>
#include <uvw.hpp>
#include <iostream>

int main()
{
    std::string url("ws://localhost:8080");
    size_t maxQueueSize = 1000;

    uvweb::PulsarClient pulsarClient(url, maxQueueSize);

    std::string tenant("public");
    std::string namespace("default");
    std::string topic("topic_name");
    std::string subscription("subscription_name");

    pulsarClient.subscribe(
        tenant,
        nameSpace,
        topic,
        subscription,
        [](const std::string& msg, const std::string& messageId) -> bool {
            std::cout << "Got message " << msg
                      << " message id: " << messageId << std::endl;
            //
            // returning true informs the client to send an acknowledge
            // to tell the Pulsar Client that a message has been properly consumed
            //
            return true;
        });

    return 0;
}
```

## Building

The project is CMake based, and all dependencies are fetched and build on the fly thanks to CMake FetchContent.

```
mkdir build
cd build
cmake ..
make
```

Dependant unittest will be run, to build faster add those cmake options.

```
cmake \
    -DCXXOPTS_BUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DLIBUV_BUILD_TESTS=OFF
```

