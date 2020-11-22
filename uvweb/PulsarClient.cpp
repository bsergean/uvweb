
#include "PulsarClient.h"

namespace uvweb
{
    PulsarClient::PulsarClient(const std::string& url) :
        _url(url)
    {
        ;
    }

    void PulsarClient::send(
        const std::string& str,
        const std::string& tenant,
        const std::string& nameSpace,
        const std::string& topic,
        const OnResponseCallback& callback)
    {
        callback(true);
    }
}
