#pragma once

#include <string>

std::string gzipCompress(const std::string& str);
bool gzipDecompress(const std::string& in, std::string& out);
