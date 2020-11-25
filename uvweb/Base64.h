
#pragma once

#include <string>

namespace uvweb
{
    std::string base64_encode(const std::string& data, size_t len);
    std::string base64_encode(const char* data, size_t len);
    std::string base64_decode(const std::string& encoded_string);
} // namespace uvweb
