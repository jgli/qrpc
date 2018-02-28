#ifndef QRPC_UTIL_RANDOM_H
#define QRPC_UTIL_RANDOM_H

#include <stdint.h>

namespace qrpc {

/* one random byte */	
extern uint8_t random8();

/* two random bytes */
extern uint16_t random16();

/* four random bytes */
extern uint32_t random32();

/* eight random bytes */
extern uint64_t random64();

/* a random floating point number between start and end, inclusive */
extern double random_range_double(double start, double end);

/* a random integer between start and end, inclusive */
extern uint64_t random_range(uint64_t start, uint64_t end);

} // namespace qrpc

#endif /* QRPC_UTIL_RANDOM_H */
