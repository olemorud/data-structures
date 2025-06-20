#define XCAT(a, b) a##b
#define CAT(a, b) XCAT(a,b)

#ifdef HASHMAP_VAL
	#ifndef HASHMAP_PREFIX
		#error "HASHMAP_VAL defined but not HASHMAP_PREFIX"
	#endif
    #define T HASHMAP_VAL
    #define HASHMAP_T       CAT(Hashmap_,T)
    #define HASHMAP_ENTRY_T CAT(hashmap_entry_,T)
#else
    #define T void*
    #define HASHMAP_T       Hashmap
	#define HASHMAP_PREFIX  hashmap
    #define HASHMAP_ENTRY_T hashmap_entry
#endif

#define HASHMAP_METHOD(x) CAT(CAT(HASHMAP_PREFIX,_), x)

/* ==== */

#include <string.h>

/* ==== */

#define ENTRY_COUNT 256

typedef struct HASHMAP_ENTRY_T {
    struct HASHMAP_ENTRY_T* next;
    size_t                  key_len;
    const void*             key;
    T                       val;
} HASHMAP_ENTRY_T;

typedef struct HASHMAP_T {
    struct arena*           arena;
    struct HASHMAP_ENTRY_T  entries[ENTRY_COUNT];
} HASHMAP_T;

HASHMAP_T* HASHMAP_METHOD(new)(struct arena* a);

T* HASHMAP_METHOD(insert)(HASHMAP_T* hashmap, const void* key, size_t key_len);

static inline T* HASHMAP_METHOD(sinsert)(HASHMAP_T* hashmap, const char* key)
{
    return HASHMAP_METHOD(insert)(hashmap, key, strlen(key));
}

T HASHMAP_METHOD(get)(HASHMAP_T* hashmap, const void* key, size_t key_len, T otherwise);

static inline T HASHMAP_METHOD(sget)(HASHMAP_T* hashmap, const char* key, T otherwise)
{
	return HASHMAP_METHOD(get)(hashmap, key, strlen(key), otherwise);
}

void HASHMAP_METHOD(init)(struct arena* a, HASHMAP_T* hashmap);

bool HASHMAP_METHOD(contains)(HASHMAP_T* hashmap, const void* key, size_t key_len);
