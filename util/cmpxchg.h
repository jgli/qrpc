#ifndef QRPC_UTIL_CMPXCHG_H
#define QRPC_UTIL_CMPXCHG_H

#include "src/qrpc/util/types.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/util/barrier.h"

namespace qrpc {

#define new not_keyword

/*
 * Non-existant functions to indicate usage errors at link time
 * (or compile-time if the compiler implements __compiletime_error().
 */
extern void __xchg_wrong_size(void)
    __compiletime_error("Bad argument size for xchg");
extern void __cmpxchg_wrong_size(void)
    __compiletime_error("Bad argument size for cmpxchg");
extern void __xadd_wrong_size(void)
    __compiletime_error("Bad argument size for xadd");
extern void __add_wrong_size(void)
    __compiletime_error("Bad argument size for add");

/*
 * Constants for operation sizes. On 32-bit, the 64-bit size it set to
 * -1 because sizeof will never return -1, thereby making those switch
 * case statements guaranteeed dead code which the compiler will
 * eliminate, and allowing the "missing symbol in the default case" to
 * indicate a usage error.
 */
#define __X86_CASE_B    1
#define __X86_CASE_W    2
#define __X86_CASE_L    4
#ifdef __x86_64__
#define __X86_CASE_Q    8
#else
#define	__X86_CASE_Q   -1   /* sizeof will never return -1 */
#endif

/* 
 * An exchange-type operation, which takes a value and a pointer, and
 * returns the old value.
 */
#define __xchg_op(ptr, arg, op, lock)           \
({                                              \
    __typeof__ (*(ptr)) __ret = (arg);          \
    switch (sizeof(*(ptr))) {                   \
    case __X86_CASE_B:                          \
        asm volatile (lock #op "b %b0, %1\n"    \
                : "+q" (__ret), "+m" (*(ptr))   \
                : : "memory", "cc");            \
        break;                                  \
    case __X86_CASE_W:                          \
        asm volatile (lock #op "w %w0, %1\n"    \
                : "+r" (__ret), "+m" (*(ptr))   \
                : : "memory", "cc");            \
        break;                                  \
    case __X86_CASE_L:                          \
        asm volatile (lock #op "l %0, %1\n"     \
                : "+r" (__ret), "+m" (*(ptr))   \
                : : "memory", "cc");            \
        break;                                  \
    case __X86_CASE_Q:                          \
        asm volatile (lock #op "q %q0, %1\n"    \
                : "+r" (__ret), "+m" (*(ptr))   \
                : : "memory", "cc");            \
        break;                                  \
    default:                                    \
        __ ## op ## _wrong_size();              \
    }                                           \
    __ret;                                      \
})

/*
 * Note: no "lock" prefix even on SMP: xchg always implies lock anyway.
 * Since this is generally used to protect other memory information, we
 * use "asm volatile" and "memory" clobbers to prevent gcc from moving
 * information around.
 */
#define xchg(ptr, v)    __xchg_op((ptr), (v), xchg, "")

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
#define __raw_cmpxchg(ptr, old, new, size, lock)        \
({                                                      \
    __typeof__(*(ptr)) __ret;                           \
    __typeof__(*(ptr)) __old = (old);                   \
    __typeof__(*(ptr)) __new = (new);                   \
    switch (size) {                                     \
    case __X86_CASE_B:                                  \
    {                                                   \
        volatile __u8 *__ptr = (volatile __u8 *)(ptr);  \
        asm volatile(lock "cmpxchgb %2,%1"              \
                : "=a" (__ret), "+m" (*__ptr)           \
                : "q" (__new), "0" (__old)              \
                : "memory");                            \
        break;                                          \
    }                                                   \
    case __X86_CASE_W:                                  \
    {                                                   \
        volatile __u16 *__ptr = (volatile __u16 *)(ptr);\
        asm volatile(lock "cmpxchgw %2,%1"              \
                : "=a" (__ret), "+m" (*__ptr)           \
                : "r" (__new), "0" (__old)              \
                : "memory");                            \
        break;                                          \
    }                                                   \
    case __X86_CASE_L:                                  \
    {                                                   \
        volatile __u32 *__ptr = (volatile __u32 *)(ptr);\
        asm volatile(lock "cmpxchgl %2,%1"              \
                : "=a" (__ret), "+m" (*__ptr)           \
                : "r" (__new), "0" (__old)              \
                : "memory");                            \
        break;                                          \
    }                                                   \
    case __X86_CASE_Q:                                  \
    {                                                   \
        volatile __u64 *__ptr = (volatile __u64 *)(ptr);\
        asm volatile(lock "cmpxchgq %2,%1"              \
                : "=a" (__ret), "+m" (*__ptr)           \
                : "r" (__new), "0" (__old)              \
                : "memory");                            \
        break;                                          \
    }                                                   \
    default:                                            \
        __cmpxchg_wrong_size();                         \
    }                                                   \
    __ret;                                              \
})

#define __cmpxchg(ptr, old, new, size)  \
    __raw_cmpxchg((ptr), (old), (new), (size), LOCK_PREFIX)

#define __sync_cmpxchg(ptr, old, new, size) \
    __raw_cmpxchg((ptr), (old), (new), (size), "lock; ")

#define __cmpxchg_local(ptr, old, new, size)    \
    __raw_cmpxchg((ptr), (old), (new), (size), "")

#ifdef __x86_64__

static inline void set_64bit(volatile __u64 *ptr, __u64 val)
{
    *ptr = val;
}

#define cmpxchg64(ptr, o, n)            \
({                                      \
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);  \
	cmpxchg((ptr), (o), (n));           \
})

#define cmpxchg64_local(ptr, o, n)      \
({                                      \
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);  \
	cmpxchg_local((ptr), (o), (n));     \
})

#else /* __x86_64 */

/*
 * Note: if you use set64_bit(), __cmpxchg64(), or their variants, you
 *       you need to test for the feature in boot_cpu_data.
 */

/*
 * CMPXCHG8B only writes to the target if we had the previous
 * value in registers, otherwise it acts as a read and gives us the
 * "new previous" value.  That is why there is a loop.  Preloading
 * EDX:EAX is a performance optimization: in the common case it means
 * we need only one locked operation.
 *
 * A SIMD/3DNOW!/MMX/FPU 64-bit store here would require at the very
 * least an FPU save and/or %cr0.ts manipulation.
 *
 * cmpxchg8b must be used with the lock prefix here to allow the
 * instruction to be executed atomically.  We need to have the reader
 * side to see the coherent 64bit value.
 */
static inline void set_64bit(volatile u64 *ptr, u64 value)
{
    u32 low  = value;
    u32 high = value >> 32;
    u64 prev = *ptr;
    
    asm volatile("\n1:\t"
            LOCK_PREFIX "cmpxchg8b %0\n\t"
            "jnz 1b"
            : "=m" (*ptr), "+A" (prev)
            : "b" (low), "c" (high)
            : "memory");
}

#define cmpxchg64(ptr, o, n)    \
    ((__typeof__(*(ptr)))__cmpxchg64((ptr), (unsigned long long)(o), \
                                     (unsigned long long)(n)))
#define cmpxchg64_local(ptr, o, n)  \
    ((__typeof__(*(ptr)))__cmpxchg64_local((ptr), (unsigned long long)(o), \
                                           (unsigned long long)(n)))

static inline u64 __cmpxchg64(volatile u64 *ptr, u64 old, u64 new)
{
    u64 prev;
    asm volatile(LOCK_PREFIX "cmpxchg8b %1"
            : "=A" (prev),
              "+m" (*ptr)
            : "b" ((u32)new),
              "c" ((u32)(new >> 32)),
              "0" (old)
            : "memory");
    return prev;
}

static inline u64 __cmpxchg64_local(volatile u64 *ptr, u64 old, u64 new)
{
    u64 prev;
    asm volatile("cmpxchg8b %1"
            : "=A" (prev),
              "+m" (*ptr)
            : "b" ((u32)new),
              "c" ((u32)(new >> 32)),
              "0" (old)
            : "memory");
    return prev;
}

#endif /* !__x86_64 */

#define cmpxchg(ptr, old, new)  \
    __cmpxchg(ptr, old, new, sizeof(*(ptr)))

#define sync_cmpxchg(ptr, old, new) \
    __sync_cmpxchg(ptr, old, new, sizeof(*(ptr)))

#define cmpxchg_local(ptr, old, new)    \
    __cmpxchg_local(ptr, old, new, sizeof(*(ptr)))

/*
 * xadd() adds "inc" to "*ptr" and atomically returns the previous
 * value of "*ptr".
 *
 * xadd() is locked when multiple CPUs are online
 * xadd_sync() is always locked
 * xadd_local() is never locked
 */
#define __xadd(ptr, inc, lock)  __xchg_op((ptr), (inc), xadd, lock)
#define xadd(ptr, inc)          __xadd((ptr), (inc), LOCK_PREFIX)
#define xadd_sync(ptr, inc)     __xadd((ptr), (inc), "lock; ")
#define xadd_local(ptr, inc)    __xadd((ptr), (inc), "")

#define __add(ptr, inc, lock)                   \
({                                              \
    __typeof__ (*(ptr)) __ret = (inc);          \
    switch (sizeof(*(ptr))) {                   \
    case __X86_CASE_B:                          \
        asm volatile (lock "addb %b1, %0\n"     \
                : "+m" (*(ptr)) : "qi" (inc)    \
                : "memory", "cc");              \
        break;                                  \
    case __X86_CASE_W:                          \
        asm volatile (lock "addw %w1, %0\n"     \
                : "+m" (*(ptr)) : "ri" (inc)    \
                : "memory", "cc");              \
        break;                                  \
    case __X86_CASE_L:                          \
        asm volatile (lock "addl %1, %0\n"      \
                : "+m" (*(ptr)) : "ri" (inc)    \
                : "memory", "cc");              \
        break;                                  \
    case __X86_CASE_Q:                          \
        asm volatile (lock "addq %1, %0\n"      \
                : "+m" (*(ptr)) : "ri" (inc)    \
                : "memory", "cc");              \
        break;                                  \
    default:                                    \
        __add_wrong_size();                     \
    }                                           \
    __ret;                                      \
})

/*
 * add_*() adds "inc" to "*ptr"
 *
 * __add() takes a lock prefix
 * add_smp() is locked when multiple CPUs are online
 * add_sync() is always locked
 */
#define add_smp(ptr, inc)   __add((ptr), (inc), LOCK_PREFIX)
#define add_sync(ptr, inc)  __add((ptr), (inc), "lock; ")

#define __cmpxchg_double(pfx, p1, p2, o1, o2, n1, n2)   \
({                                                      \
    bool __ret;                                         \
    __typeof__(*(p1)) __old1 = (o1), __new1 = (n1);     \
    __typeof__(*(p2)) __old2 = (o2), __new2 = (n2);     \
    BUILD_BUG_ON(sizeof(*(p1)) != sizeof(long));        \
    BUILD_BUG_ON(sizeof(*(p2)) != sizeof(long));        \
    asm volatile(pfx "cmpxchg%c4b %2; sete %0"          \
            : "=a" (__ret), "+d" (__old2),              \
              "+m" (*(p1)), "+m" (*(p2))                \
            : "i" (2 * sizeof(long)), "a" (__old1),     \
              "b" (__new1), "c" (__new2));              \
    __ret;                                              \
})

#define cmpxchg_double(p1, p2, o1, o2, n1, n2)  \
    __cmpxchg_double(LOCK_PREFIX, p1, p2, o1, o2, n1, n2)

#define cmpxchg_double_local(p1, p2, o1, o2, n1, n2)    \
    __cmpxchg_double(, p1, p2, o1, o2, n1, n2)

#undef new

} // namespace qrpc

#endif	/* QRPC_UTIL_CMPXCHG_H */
