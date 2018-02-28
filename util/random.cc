#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <algorithm>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/util/random.h"

using namespace std;

namespace qrpc {

/*
 * Internal scratch state used by random_r 128 is the same size as
 * initstate() uses for regular random(), see manpages for details.
 * statebuf is malloc'ed and this memory is leaked, it could be a __thread
 * buffer, but after running into linker issues with large thread local
 * storage buffers, we thought better.
 */
#define STATE_BYTES 128
static char* statebuf;
//__thread char* statebuf;

/*
 * random_r's state, must be handed to each call, and seems to refer to
 * statebuf in some undocumented way.
 */
static struct random_data randbuf;
//__thread struct random_data randbuf;

static void init_random() __attribute__((constructor));
static void exit_random() __attribute__((destructor));

/**
 * Initialize thread-local state for random number generator.
 * Must be called before any random numbers are generated.
 */
static void init_random()
{
    unsigned int seed;
    ssize_t bytes_read;
    int fd;

    //if (likely(statebuf != NULL)) {
    //    return;
    //}
    if (statebuf != NULL) {
        LOG(FATAL) << "the random has been initialized";
    }
    
    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        LOG(FATAL) << "open /dev/urandom failed!!!, errno: " << errno;
    }
    
    bytes_read = read(fd, &seed, sizeof(seed));
    close(fd);
    
    if (bytes_read != sizeof(seed)) {
        LOG(FATAL) << "read /dev/urandom failed!!!, errno: " << errno;
    }
    
    statebuf = (char *)malloc(STATE_BYTES);
    if (statebuf == NULL) {
        LOG(FATAL) << "no memory";
    }
    
    initstate_r(seed, statebuf, STATE_BYTES, &randbuf);
}

static void exit_random()
{
    assert(statebuf != NULL);

    free(statebuf);
    statebuf = NULL;
}

#define get_random_bytes(type)                  \
({                                              \
    type buf;                                   \
    size_t offset = 0;                          \
                                                \
    while (offset < sizeof(buf)) {              \
        uint64_t r = random64();                \
        size_t copy = min(sizeof(r),            \
                          sizeof(buf) - offset);\
        memcpy((char *)&buf + offset, &r, copy);\
        offset += copy;                         \
    }                                           \
    buf;                                        \
})

/* Return a random number between 0 and 1 */
static inline double random_unit()
{
    return (double)random64() / (double)ULLONG_MAX;
}

uint8_t random8()
{
    return get_random_bytes(uint8_t);
}

uint16_t random16()
{
    return get_random_bytes(uint16_t);
}

uint32_t random32()
{
    return get_random_bytes(uint32_t);
}

uint64_t random64()
{
    /*
     * Each call to random returns 31 bits of randomness,
     * so we need three to get 64 bits of randomness.
     */
    int32_t lo, mid, hi;
    
    //init_random();
    assert(statebuf != NULL);
    
    random_r(&randbuf, &lo);
    random_r(&randbuf, &mid);
    random_r(&randbuf, &hi);
    
    uint64_t r = (((uint64_t)lo & 0x00000003)        | // NOLINT
                 (((uint64_t)mid & 0x7FFFFFFF) << 2) | // NOLINT
                 (((uint64_t)hi & 0x7FFFFFFF) << 33)); // NOLINT
    
    return r;
}

double random_range_double(double start, double end)
{
    return start + random_unit()  * (end - start);
}

uint64_t random_range(uint64_t start, uint64_t end)
{
    return lround(random_range_double(start, end));
}

} // namespace qrpc
