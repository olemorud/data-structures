
#ifdef GNU // ? ? ?

#define never_inline __attribute__((noinline))

#define always_inline __attribute__((always_inline))

/* Hint to the compiler that a condition is likely.
 * Example:
 *   if (likely(a == 5)) ... */
#define likely(x) __builtin_expect(!!(x), 1)

/* Hint to the compiler that a condition is unlikely.
 * Useful for non-critical paths such as logging an error.
 * Example:
 *   if (unlikely(errno != 0)) ... */
#define unlikely(x) __builtin_expect(!!(x), 0)

/* Store data in read-only sections, causing the program to crash 
 * if writes are done. */
#define readonly

#endif
