#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define WRAP_CSTR(cstr) (struct str8_chunk){.len = strlen(cstr), .data = cstr, .next = NULL}

typedef int (*char_transform_func)(int);

void* return_null()
{
    return NULL;
}

void *(*arena_alloc)(size_t) = return_null;

struct str8_chunk {
    size_t             len;
    const char*        data;
    struct str8_chunk* next;
};

struct str8 {
    size_t len;
    struct str8_chunk*  head;
    struct str8_chunk*  tail;
    char_transform_func transformations[];
};

struct str8_chunk* chunk_from_cstr(const char* n, size_t n)
{
    /* if allocations fail, just return a stub instead of bubbling an error or
     * crashing the program */
    #define STUB_MSG "[str8 memory allocation error]"
    static struct str8_chunk error_chunk = {
        .len  = sizeof(STUB_MSG)-1,
        .data = STUB_MSG,
        .next = NULL,
    };
    static struct str8_chunk stub = {
        .len  = 0,
        .data = "",
        .next = &error_chunk,
    };

    struct str8_chunk* next_chunk = arena_alloc(n);
    if (!next_chunk) {
        next_chunk = &stub;
    } else {
        next_chunk->len = n;
        next_chunk->data = tail;
        next_chunk->next = NULL;
    }
    return next_chunk;
}

void str8_append(struct str8* s, const char* tail)
{
    const size_t n = strlen(tail);
    struct str8_chunk* next_chunk = chunk_from_cstr(tail, n);

    if (s->head == NULL) {
        s->head = next_chunk;
        s->tail = next_chunk;
    } else {
        s->tail->next = next_chunk;
        s->tail = next_chunk;
    }

    s->len += n;
}

struct str8 str8_new(const char* str)
{
    struct str8 s = {0};

    str8_append(&s, str);

    return s;
}

void str8_print(FILE* out, const struct str8* str)
{
    struct str8_chunk* s = &(struct str8_chunk){.next = str->head};
    while (s = s->next) {
        fprintf(out, "%*s", s->len, s->data);
    }
}

int main()
{
    struct str8 s = str8_new("hello");

    str8_append(&s, " world");

    str8_print(stdout, &s);

    return EXIT_SUCCESS;
}
