#ifndef QRPC_UTIL_BARRIER_H
#define QRPC_UTIL_BARRIER_H

#include "src/qrpc/util/types.h"
#include "src/qrpc/util/compiler.h"

namespace qrpc {

#ifdef CONFIG_SMP
#error "!!! redefine CONFIG_SMP"
#else
#define CONFIG_SMP 1
#endif

#ifdef CONFIG_SMP
#define LOCK_PREFIX	"lock ; "
#else
#define LOCK_PREFIX
#endif

/**
 * General memory barrier.
 *
 * Guarantees that the LOAD and STORE operations generated before the
 * barrier occur before the LOAD and STORE operations generated after.
 */
#ifdef __x86_64__
#define mb()    asm volatile("mfence":::"memory")
#else
#define	mb()    _mm_mfence()
#endif

/**
 * Write memory barrier.
 *
 * Guarantees that the STORE operations generated before the barrier
 * occur before the STORE operations generated after.
 */
#ifdef __x86_64__
#define wmb()   asm volatile("sfence" ::: "memory")
#else
#define	wmb()   _mm_sfence()
#endif

/**
 * Read memory barrier.
 *
 * Guarantees that the LOAD operations generated before the barrier
 * occur before the LOAD operations generated after.
 */
#ifdef __x86_64__
#define rmb()   asm volatile("lfence":::"memory")
#else
#define	rmb()   _mm_lfence()
#endif

/**
 * read_barrier_depends - Flush all pending reads that subsequents reads
 * depend on.
 *
 * No data-dependent reads from memory-like regions are ever reordered
 * over this barrier.  All reads preceding this primitive are guaranteed
 * to access memory (but not necessarily other CPUs' caches) before any
 * reads following this primitive that depend on the data return by
 * any of the preceding reads.  This primitive is much lighter weight than
 * rmb() on most CPUs, and is never heavier weight than is
 * rmb().
 *
 * These ordering constraints are respected by both the local CPU
 * and the compiler.
 *
 * Ordering is not guaranteed by anything other than these primitives,
 * not even by data dependencies.  See the documentation for
 * memory_barrier() for examples and URLs to more information.
 *
 * For example, the following code would force ordering (the initial
 * value of "a" is zero, "b" is one, and "p" is "&a"):
 *
 * <programlisting>
 *	CPU 0				CPU 1
 *
 *	b = 2;
 *	memory_barrier();
 *	p = &b;				q = p;
 *					read_barrier_depends();
 *					d = *q;
 * </programlisting>
 *
 * because the read of "*q" depends on the read of "p" and these
 * two reads are separated by a read_barrier_depends().  However,
 * the following code, with the same initial values for "a" and "b":
 *
 * <programlisting>
 *	CPU 0				CPU 1
 *
 *	a = 2;
 *	memory_barrier();
 *	b = 3;				y = b;
 *					read_barrier_depends();
 *					x = a;
 * </programlisting>
 *
 * does not enforce ordering, since there is no data dependency between
 * the read of "a" and the read of "b".  Therefore, on some CPUs, such
 * as Alpha, "y" could be set to 3 and "x" to 0.  Use rmb()
 * in cases like this where there are no data dependencies.
 **/

#define read_barrier_depends()	do { } while (0)

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()   barrier()
#define smp_wmb()   barrier()
#define smp_read_barrier_depends()  read_barrier_depends()
#define set_mb(var, value)  do { (void)xchg(&var, value); } while (0)
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define smp_read_barrier_depends()  do { } while (0)
#define set_mb(var, value)  do { var = value; barrier(); } while (0)
#endif

#undef CONFIG_SMP

} // namespace qrpc

#endif /* SRC_QRPC_UTIL_BARRIER_H */
