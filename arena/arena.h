
#pragma once
#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>

#ifndef ARENA_MMAP_SIZE
#define ARENA_MMAP_SIZE (1UL << 36UL)
#endif

#ifndef ARENA_ALIGNMENT
#define ARENA_ALIGNMENT (sizeof(unsigned char*))
_Static_assert((ARENA_ALIGNMENT & (ARENA_ALIGNMENT - 1)) == 0,
               "ARENA_ALIGNMENT must be power of two");
#endif

// flags = 0 should be considered the default
enum arena_flags {
    ARENA_DONTALIGN = 1<<0,
    ARENA_MMAPED    = 1<<1,
};

typedef struct arena {
    unsigned char*  data;
    size_t size;
    size_t cap;
    unsigned char   flags;
} Arena;

/**
 * Allocate a new arena.
 * The underlying memory is allocated with mmap.
 * Errors can be checked with `arena_new_failed()`
 */
Arena arena_new();

/**
 * Delete memory mapped for arena.
 * Should only be used with arenas from arena_new().
 * Returns 0 on success, -1 on failure
 */
int arena_delete(Arena *a);

/**
 * Attach an arena to an existing memory region.
 * The arena will not expand the region if space is exceeded.
 */
static inline Arena arena_attach(void *ptr, size_t size)
{
    return (Arena) { .data = ptr, .size = 0, .cap = size, .flags = 0 };
}

/**
 * Detach an arena from an existing memory region.
 */
static inline void *arena_detatch(Arena arena)
{
    return arena.data;
}

/**
 * Returns true if creating new arena failed
 */
static inline bool arena_new_failed(Arena *a)
{
    return a->data == NULL;
}

/**
 * Reset an arena.
 */
static inline void arena_reset(Arena *a)
{
    a->size = 0;
}

/**
 * Allocate memory from an arena.
 * Returns NULL and sets errno on failure.
 */
void *arena_alloc(Arena *a, size_t len);

/**
 * Allocate and zero memory from an arena.
 * Returns NULL and sets errno on failure.
 */
void *arena_calloc(Arena *a, size_t nmemb, size_t size);

#endif
