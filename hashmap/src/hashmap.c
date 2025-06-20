
#define _POSIX_C_SOURCE 200809L

#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 202112L
    #define constexpr const
#endif

#include "arena.h"
#include "hashmap.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Because this file might be included several times in tests (to test static functions),
 * non-templated functions are in a "header guard" */
#ifndef HASHMAP_HASHES
#define HASHMAP_HASHES

static size_t hash_djb2(const void* key, size_t len)
{
    /* djb2 inspired hash */
    constexpr size_t seed  = 5381;
    constexpr size_t magic = 33;

    size_t n = seed;

    const uint8_t* key_bytewise = key;

    for (size_t i = 0; i < len; i++) {
        n = n * magic + key_bytewise[i];
    }
    return n;
}

static size_t jenkins_one_at_a_time_hash(const void* data, size_t len) {
    size_t i = 0;
    size_t h = 0;
    const uint8_t* key = data;
    while (i != len) {
        h += key[i++];
        h += h << 10;
        h ^= h >> 6;
    }
    h += h << 3;
    h ^= h >> 11;
    h += h << 15;
    return h;
}
#endif /* ifdef HASHMAP_HASHES */

// Returns a pointer pointer to the entry matching the key. If the dereferenced
// return value is null it can simply be malloced with no additional
// linked-list logic.
//
// This interface is kind of horrible but it's only used internally. The
// parameter `dummy` must be defined for the duration of the return value of
// this function.
static inline HASHMAP_ENTRY_T** HASHMAP_METHOD(at)(HASHMAP_T* hashmap, const void* key, size_t key_len, HASHMAP_ENTRY_T* dummy)
{
    size_t h = hash_djb2(key, key_len);
    HASHMAP_ENTRY_T* e = &(hashmap->entries[h % ENTRY_COUNT]);
	dummy->next = e;
    HASHMAP_ENTRY_T** indirect = &(dummy->next);

    while ((e = *indirect) /* implicitly tests e == NULL */
		   && (e->key_len != key_len
		   || memcmp(e->key, key, key_len) != 0))
    {
        indirect = &(e->next);
    }

    return indirect;
}

T* HASHMAP_METHOD(insert)(HASHMAP_T* hashmap, const void* key, size_t key_len)
{
	HASHMAP_ENTRY_T dummy = {0};
    HASHMAP_ENTRY_T** entry_indirect = HASHMAP_METHOD(at)(hashmap, key, key_len, &dummy);

    if (*entry_indirect == NULL) {
        *entry_indirect = arena_calloc(hashmap->arena, sizeof **entry_indirect, 1);
        (*entry_indirect)->key = arena_copy(hashmap->arena, key, key_len);
        (*entry_indirect)->key_len = key_len;
    }

    return &((*entry_indirect)->val);
}

T HASHMAP_METHOD(get)(HASHMAP_T* hashmap, const void* key, size_t key_len, T otherwise)
{
	HASHMAP_ENTRY_T dummy = {0};
    HASHMAP_ENTRY_T** entry_indirect = HASHMAP_METHOD(at)(hashmap, key, key_len, &dummy);
    if (*entry_indirect == NULL) {
        return otherwise;
    }
    return (*entry_indirect)->val;
}

HASHMAP_T* HASHMAP_METHOD(new)(struct arena* a)
{
    HASHMAP_T* hm = arena_calloc(a, sizeof *hm, 1);

    hm->arena = a;

    return hm;
}

void HASHMAP_METHOD(init)(struct arena* a, HASHMAP_T* hashmap)
{
	hashmap->arena = a;
}

bool HASHMAP_METHOD(contains)(HASHMAP_T* hashmap, const void* key, size_t key_len)
{
	HASHMAP_ENTRY_T dummy = {0};
    HASHMAP_ENTRY_T** entry_indirect = HASHMAP_METHOD(at)(hashmap, key, key_len, &dummy);
	return *entry_indirect != NULL;
}
