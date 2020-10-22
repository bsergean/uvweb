
#include "gzip.h"
#include <array>
#include <zlib.h>

bool gzipDecompress(const std::string& in, std::string& out)
{
    z_stream inflateState;
    memset(&inflateState, 0, sizeof(inflateState));

    inflateState.zalloc = Z_NULL;
    inflateState.zfree = Z_NULL;
    inflateState.opaque = Z_NULL;
    inflateState.avail_in = 0;
    inflateState.next_in = Z_NULL;

    if (inflateInit2(&inflateState, 16 + MAX_WBITS) != Z_OK)
    {
        return false;
    }

    inflateState.avail_in = (uInt) in.size();
    inflateState.next_in = (unsigned char*) (const_cast<char*>(in.data()));

    const int kBufferSize = 1 << 14;
    std::array<unsigned char, kBufferSize> compressBuffer;

    do
    {
        inflateState.avail_out = (uInt) kBufferSize;
        inflateState.next_out = &compressBuffer.front();

        int ret = inflate(&inflateState, Z_SYNC_FLUSH);

        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
        {
            inflateEnd(&inflateState);
            return false;
        }

        out.append(reinterpret_cast<char*>(&compressBuffer.front()),
                   kBufferSize - inflateState.avail_out);
    } while (inflateState.avail_out == 0);

    inflateEnd(&inflateState);
    return true;
}
