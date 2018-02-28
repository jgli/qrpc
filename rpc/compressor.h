#ifndef QRPC_RPC_COMPRESSOR_H
#define QRPC_RPC_COMPRESSOR_H

#include <stdint.h>
#include <pthread.h>

namespace qrpc {

enum CompressionStatus {
    kCompOk             = 0,
    kCompInvalidInput   = 1,
    kCompBufferTooSmall = 2
};

class Compressor {
public:
    Compressor();
    ~Compressor();

    char* ExpandBufferCache(size_t len);
    void UseCompression(CompressionType type) { type_ = type; }

    int Compress(const char *in, size_t ilen, char *out, size_t olen, size_t *rlen);
    int Uncompress(const char *in, size_t ilen, char *out, size_t olen, size_t *rlen);

private:
    int ZlibCompress(const char *in, size_t ilen, char *out, size_t olen, size_t *rlen);
    int ZlibUncompress(const char *in, size_t ilen, char *out, size_t olen, size_t *rlen);

    int Lz4Compress(const char *in, size_t ilen, char *out, size_t olen, size_t *rlen);
    int Lz4Uncompress(const char *in, size_t ilen, char *out, size_t olen, size_t *rlen);

    int SnappyCompress(const char *in, size_t ilen, char *out, size_t olen, size_t *rlen);
    int SnappyUncompress(const char *in, size_t ilen, char *out, size_t olen, size_t *rlen);

private:
    char *buffer_;
    size_t len_;
    CompressionType type_;

private:
    /* No copying allowed */
    Compressor(const Compressor &);
    void operator=(const Compressor &);
};

} // namespace qrpc

#endif /* QRPC_RPC_COMPRESSOR_H */
