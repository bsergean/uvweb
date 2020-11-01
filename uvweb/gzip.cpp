
#include "gzip.h"
#include <array>
#include <libdeflate.h>

std::string gzipCompress(const std::string& str)
{
    int compressionLevel = 6;
    struct libdeflate_compressor* compressor;

    compressor = libdeflate_alloc_compressor(compressionLevel);

    const void* uncompressed_data = str.data();
    size_t uncompressed_size = str.size();
    void* compressed_data;
    size_t actual_compressed_size;
    size_t max_compressed_size;

    max_compressed_size = libdeflate_gzip_compress_bound(compressor, uncompressed_size);
    compressed_data = malloc(max_compressed_size);

    if (compressed_data == NULL)
    {
        return std::string();
    }

    actual_compressed_size = libdeflate_gzip_compress(
        compressor, uncompressed_data, uncompressed_size, compressed_data, max_compressed_size);

    libdeflate_free_compressor(compressor);

    if (actual_compressed_size == 0)
    {
        free(compressed_data);
        return std::string();
    }

    std::string out;
    out.assign(reinterpret_cast<char*>(compressed_data), actual_compressed_size);
    free(compressed_data);

    return out;
}

uint32_t loadDecompressedGzipSize(const uint8_t* p)
{
    return ((uint32_t) p[0] << 0) | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) |
        ((uint32_t) p[3] << 24);
}

bool gzipDecompress(const std::string& in, std::string& out)
{
    struct libdeflate_decompressor* decompressor;
    decompressor = libdeflate_alloc_decompressor();

    const void* compressed_data = in.data();
    size_t compressed_size = in.size();

    // Retrieve uncompressed size from the trailer of the gziped data
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&in.front());
    auto uncompressed_size = loadDecompressedGzipSize(&ptr[compressed_size - 4]);

    // Use it to redimension our output buffer
    out.resize(uncompressed_size);

    libdeflate_result result = libdeflate_gzip_decompress(
            decompressor, compressed_data, compressed_size, &out.front(), uncompressed_size, NULL);

    libdeflate_free_decompressor(decompressor);
    return result == LIBDEFLATE_SUCCESS;
}
