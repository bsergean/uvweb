# uvweb

Http, WebSocket code based on uvw library and http_parser.

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
            return true;
        });

    return 0;
}
```
