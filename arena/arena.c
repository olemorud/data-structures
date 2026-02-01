
#include "arena.h"

#include <stddef.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

struct arena arena_new()
{
    void *p = mmap(NULL, ARENA_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) {
        goto mmap_failed;
    }

    struct arena a = {
        .data = p,
        .size = 0,
        .cap = ARENA_MMAP_SIZE,
        .flags = ARENA_MMAPED
    };

    return a;

mmap_failed:
    return (struct arena) { 0 };
}

void* arena_alloc(struct arena *a, size_t size)
{
    // align
    if (!(a->flags & ARENA_DONTALIGN)) {
        size = (size + ARENA_ALIGNMENT - 1) & ~(ARENA_ALIGNMENT - 1);
    }

    unsigned char* p = a->data + a->size;
    if (size > a->cap - a->size) {
        return NULL;
    }
    a->size += size;
    return (void*)p;
}

void* arena_calloc(struct arena *a, size_t nmemb, size_t size)
{
    size_t product;
    if (__builtin_mul_overflow(nmemb, size, &product)) {
        return NULL;
    }
    void *p = arena_alloc(a, product);
    if (p == NULL)
        return p;
    memset(p, 0, product);
    return p;
}

int arena_delete(struct arena *a)
{
    if (a->flags & ARENA_MMAPED) {
        int ok = munmap(a->data, a->cap);
        if (ok == -1) {
            /* should actually never happen */
            return -1;
        }
        a->cap  = 0;
        a->size = 0;
        return 0;
    } else {
        return -1;
    }
}
