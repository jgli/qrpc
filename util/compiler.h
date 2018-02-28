#ifndef QRPC_UTIL_COMPILER_H
#define QRPC_UTIL_COMPILER_H

#include <stddef.h>
#include "src/qrpc/util/log.h"

namespace qrpc {

/** Optimization barrier */
#define barrier() __asm__ __volatile__("": : :"memory")

/**
 * Check if a branch is likely to be taken.
 *
 * This compiler builtin allows the developer to indicate
 * if a branch is likely to be taken. Example:
 *
 *   if (likely(x > 1))
 *      do_stuff();
 */
#ifndef likely
#define likely(x)    __builtin_expect(!!(x), 1)
#endif

/**
 * Check if a branch is unlikely to be taken.
 *
 * This compiler builtin allows the developer to indicate
 * if a branch is unlikely to be taken. Example:
 *
 *   if (unlikely(x < 1))
 *      do_stuff();
 */
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif

#if __GNUC_MINOR__ > 0
#define __compiletime_object_size(obj) __builtin_object_size(obj, 0)
#endif

#if __GNUC_MINOR__ >= 4
#define __compiletime_warning(message) __attribute__((warning(message)))
#define __compiletime_error(message) __attribute__((error(message)))
#endif

/* Compile time object size, -1 for unknown */
#ifndef __compiletime_object_size
#define __compiletime_object_size(obj) -1
#endif

#ifndef __compiletime_warning
#define __compiletime_warning(message)
#endif

#ifndef __compiletime_error
#define __compiletime_error(message)
#define __compiletime_error_fallback(condition) \
    do { ((void)sizeof(char[1 - 2 * condition])); } while (0)
#else 
#define __compiletime_error_fallback(condition) do { } while (0)
#endif

#define __compiletime_assert(condition, msg, prefix, suffix)    \
do {                                                            \
    bool __cond = !(condition);                                 \
    extern void prefix ## suffix(void) __compiletime_error(msg);\
    if (__cond)                                                 \
        prefix ## suffix();                                     \
    __compiletime_error_fallback(__cond);                       \
} while (0)

#define _compiletime_assert(condition, msg, prefix, suffix) \
    __compiletime_assert(condition, msg, prefix, suffix)

/**
 * compiletime_assert - break build and emit msg if condition is false
 * @condition: a compile-time constant condition to check
 * @msg:       a message to emit if condition is false
 *
 * In tradition of POSIX assert, this macro will break the build if the
 * supplied condition is *false*, emitting the supplied error message if the
 * compiler has support to do so.
 */
#define compiletime_assert(condition, msg) \
    _compiletime_assert(condition, msg, __compiletime_assert_, __LINE__)

/**
 * Don't use BUG() or BUG_ON() unless there's really no way out; one
 * example might be detecting data structure corruption in the middle
 * of an operation that can't be backed out of.  If the (sub)system
 * can somehow continue operating, perhaps with reduced functionality,
 * it's probably not BUG-worthy.
 *
 * If you're tempted to BUG(), think again:  is completely giving up
 * really the *only* solution?  There are usually better options, where
 * users don't need to reboot ASAP and can mostly shut down cleanly.
 */
#define BUG()                           \
do {                                    \
    LOG(FATAL) << "serious fatal!!!";   \
} while (0)

#define BUG_ON(condition) do { if (unlikely(condition)) BUG(); } while(0)

#define BUG_MSG(msg)                    \
do {                                    \
    LOG(FATAL) << (msg);                \
} while (0)

#define BUG_ON_MSG(cond, msg) do { if (unlikely(cond)) BUG_MSG(msg); } while(0)

/**
 * BUILD_BUG_ON_MSG - break compile if a condition is true & emit supplied
 *		      error message.
 * @condition: the condition which the compiler should know is false.
 *
 * See BUILD_BUG_ON for description.
 */
#define BUILD_BUG_ON_MSG(cond, msg) compiletime_assert(!(cond), msg)

/**
 * BUILD_BUG_ON - break compile if a condition is true.
 * @condition: the condition which the compiler should know is false.
 *
 * If you have some code which relies on certain constants being equal, or
 * some other compile-time-evaluated condition, you should use BUILD_BUG_ON to
 * detect if someone changes it.
 *
 * The implementation uses gcc's reluctance to create a negative array, but gcc
 * (as of 4.4) only emits that error for obvious cases (e.g. not arguments to
 * inline functions).  Luckily, in 4.3 they added the "error" function
 * attribute just for this type of case.  Thus, we use a negative sized array
 * (should always create an error on gcc versions older than 4.4) and then call
 * an undefined function with the error attribute (should always create an
 * error on gcc 4.3 and later).  If for some reason, neither creates a
 * compile-time error, we'll still have a link-time error, which is harder to
 * track down.
 */
#ifndef NDEBUG
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#else
#define BUILD_BUG_ON(condition) \
    BUILD_BUG_ON_MSG(condition, "BUILD_BUG_ON failed: " #condition)
#endif

/* Force a compilation error if a constant expression is not a power of 2 */
#define BUILD_BUG_ON_NOT_POWER_OF_2(n)  \
    BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))

/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON_NULL(e) ((void *)sizeof(struct { int:-!!(e); }))

/*
 * BUILD_BUG_ON_INVALID() permits the compiler to check the validity of the
 * expression but avoids the generation of any code, even if that expression
 * has side-effects.
 */
#define BUILD_BUG_ON_INVALID(e) ((void)(sizeof((long)(e))))

/**
 * BUILD_BUG - break compile if used.
 *
 * If you have some code that you expect the compiler to eliminate at
 * build time, you should use BUILD_BUG to detect if it is
 * unexpectedly used.
 */
#define BUILD_BUG() BUILD_BUG_ON_MSG(1, "BUILD_BUG failed!!!")

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

} // namespace qrpc

#endif /* QRPC_UTIL_COMPILER_H */
