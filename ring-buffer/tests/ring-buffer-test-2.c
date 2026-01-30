// test_rb_int_128_overflow.c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define RB_T    int
#define RB_SIZE 128
#include "ring-buffer.h"

int main(void)
{
    struct rb_int_128 rb;
    rb_int_128_init(&rb);

    long long expected = 0;
    long long got = 0;

    for (int i = 0; i < 128; i++) {
        bool ok = rb_int_128_enqueue(&rb, i);
        if (!ok) {
            fprintf(stderr, "rb_int_128_enqueue: failed to insert to available queue\n");
            return EXIT_FAILURE;
        }
        expected += i;
    }

    for (int i = 0; i < 1024; i++) {
        bool ok = rb_int_128_enqueue(&rb, 1000000000 + i);
        if (ok) {
            fprintf(stderr, "rb_int_128_enqueue: inserted into full queue\n");
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < 128; i++) {
        int v;
        bool ok = rb_int_128_dequeue(&rb, &v);
        if (!ok) {
            fprintf(stderr, "rb_int_128_dequeue: failed to dequeue\n");
            return EXIT_FAILURE;
        }
        got += v;
    }

    int v;
    if (rb_int_128_dequeue(&rb, &v)) {
        return EXIT_FAILURE;
    }

    if (got != expected) {
        fprintf(stderr, "FAIL: expected=%lld got=%lld\n", expected, got);
        return EXIT_FAILURE;
    }

    printf("PASS: sum=%lld\n", got);
    return EXIT_SUCCESS;
}

