#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string>

#include <snappy.h>
#include <lz4.h>
#include <zlib.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/rpc/controller.h"
#include "src/qrpc/rpc/compressor.h"

using namespace std;

namespace qrpc {

Compressor::Compressor()
    : buffer_(NULL)
    , len_(0)
    , type_(kNoCompression)
{

}

Compressor::~Compressor()
{
    free(buffer_);
}

char* Compressor::ExpandBufferCache(size_t len)
{
    if (len <= len_) {
        return buffer_;
    }

    if (!len_) { len_ = 4096; }
    for (; len_ < len; ) { len_ *= 2; }

    buffer_ = (char *)realloc(buffer_, len_);
    if (!buffer_) {
        LOG(FATAL) << "out of memory!!!";
    }

    return buffer_;
}

int __always_inline
Compressor::ZlibCompress(const char *in, size_t ilen,
                         char *out, size_t olen, size_t *rlen)
{
    unsigned long required = compressBound(ilen);

    if (required > olen) {
        return kCompBufferTooSmall;
    }

    *rlen = olen;
    int rc = compress2((Bytef *)out, rlen, (const Bytef *)in, ilen, Z_BEST_SPEED);

    switch (rc) {
    case Z_OK:
        rc = kCompOk;
        break;
    case Z_BUF_ERROR:
        rc = kCompBufferTooSmall;
        break;
    case Z_MEM_ERROR:
        LOG(FATAL) << "out of memory";
        break;
    case Z_STREAM_ERROR:
        LOG(FATAL) << "invalid compression level";
        break;
    default:
        LOG(FATAL) << "unknown error number from zlib: " << rc;
        break;
    }

    return rc;
}

int __always_inline
Compressor::ZlibUncompress(const char *in, size_t ilen,
                           char *out, size_t olen, size_t *rlen)
{
    *rlen = olen;
    int rc = uncompress((Bytef *)out, rlen, (const Bytef *)in, ilen);

    switch (rc) {
    case Z_OK:
        rc = kCompOk;
        break;
    case Z_BUF_ERROR:
        rc = kCompBufferTooSmall;
        break;
    case Z_DATA_ERROR:
        rc = kCompInvalidInput;
        break;
    case Z_MEM_ERROR:
        LOG(FATAL) << "out of memory";
        break;
    default:
        LOG(FATAL) << "unknown error number from zlib: " << rc;
        break;
    }

    return rc;
}

int __always_inline
Compressor::Lz4Compress(const char *in, size_t ilen,
                        char *out, size_t olen, size_t *rlen)
{
    if (ilen > LZ4_MAX_INPUT_SIZE) {
        LOG(ERROR) << "the input stream is too large";
        return kCompInvalidInput;
    }

    size_t required = LZ4_compressBound(ilen);

    if (required > olen) {
        return kCompBufferTooSmall;
    }

    *rlen = LZ4_compress_default(in, out, ilen, olen);

    return (*rlen > 0 ? kCompOk : kCompBufferTooSmall);
}

int __always_inline
Compressor::Lz4Uncompress(const char *in, size_t ilen,
                          char *out, size_t olen, size_t *rlen)
{
    int rc = LZ4_decompress_safe(in, out, ilen, olen);

    if (rc < 0) {
        return kCompBufferTooSmall;
    }

    *rlen = rc;
    return kCompOk;
}

int __always_inline
Compressor::SnappyCompress(const char *in, size_t ilen,
                           char *out, size_t olen, size_t *rlen)
{
    size_t required = snappy::MaxCompressedLength(ilen);

    if (required > olen) {
        return kCompBufferTooSmall;
    }

    snappy::RawCompress(in, ilen, out, rlen);

    return kCompOk;
}

int __always_inline
Compressor::SnappyUncompress(const char *in, size_t ilen,
                             char *out, size_t olen, size_t *rlen)
{
    bool rc = snappy::GetUncompressedLength(in, ilen, rlen);

    if (!rc) {
        return kCompInvalidInput;
    }

    if (*rlen > olen) {
        return kCompBufferTooSmall;
    }

    rc = snappy::RawUncompress(in, ilen, out);

    return (rc ? kCompOk : kCompInvalidInput);
}

int Compressor::Compress(const char *in, size_t ilen,
                         char *out, size_t olen, size_t *rlen)
{
    int rc = kCompOk;

    switch (type_) {
    case kSnappyCompression:
        rc = SnappyCompress(in, ilen, out, olen, rlen);
        break;
    case kLz4Compression:
        rc = Lz4Compress(in, ilen, out, olen, rlen);
        break;
    case kZlibCompression:
        rc = ZlibCompress(in, ilen, out, olen, rlen);
        break;
    case kNoCompression:
    default:
        LOG(FATAL) << "invalid compression context";
        break;
    }

    return rc;
}

int Compressor::Uncompress(const char *in, size_t ilen,
                           char *out, size_t olen, size_t *rlen)
{
    int rc = kCompOk;

    switch (type_) {
    case kSnappyCompression:
        rc = SnappyUncompress(in, ilen, out, olen, rlen);
        break;
    case kLz4Compression:
        rc = Lz4Uncompress(in, ilen, out, olen, rlen);
        break;
    case kZlibCompression:
        rc = ZlibUncompress(in, ilen, out, olen, rlen);
        break;
    case kNoCompression:
    default:
        LOG(FATAL) << "invalid compression context";
        break;
    }

    return rc;
}

} // namespace qrpc
